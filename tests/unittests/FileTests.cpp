// SPDX-FileCopyrightText: Michael Popoloski
// SPDX-License-Identifier: MIT

#include "Test.h"
#include <fstream>

#include "slang/text/Glob.h"
#include "slang/text/SourceManager.h"
#include "slang/util/String.h"

std::string getTestInclude() {
    return findTestDir() + "/include.svh";
}

TEST_CASE("Read source") {
    SourceManager manager;
    std::string testPath = getTestInclude();

    CHECK(!manager.readSource("X:\\nonsense.txt"));

    auto file = manager.readSource(testPath);
    REQUIRE(file);
    CHECK(file->data.length() > 0);
}

TEST_CASE("Read header (absolute)") {
    SourceManager manager;
    std::string testPath = getTestInclude();

    // check load failure
    auto result = manager.readHeader("X:\\nonsense.txt", SourceLocation(), nullptr, false, {});
    CHECK(!result);
    CHECK(result.error() == std::errc::no_such_file_or_directory);

    // successful load
    auto buffer1 = manager.readHeader(testPath, SourceLocation(), nullptr, false, {});
    REQUIRE(buffer1);
    CHECK(!buffer1->data.empty());

    // next load should be cached
    auto buffer2 = manager.readHeader(testPath, SourceLocation(), nullptr, false, {});
    REQUIRE(buffer2);
    CHECK(!buffer2->data.empty());
    CHECK(buffer1->data.data() == buffer2->data.data());
}

TEST_CASE("Read header (relative)") {
    SourceManager manager;

    // relative to nothing should never return anything
    auto result = manager.readHeader("relative", SourceLocation(), nullptr, false, {});
    CHECK(!result);
    CHECK(result.error() == std::errc::no_such_file_or_directory);

    // get a file ID to load relative to
    auto buffer1 = manager.readHeader(getTestInclude(), SourceLocation(), nullptr, false, {});
    REQUIRE(buffer1);

    // reading the same header by name should return the same data pointer
    auto buffer2 = manager.readHeader("include.svh", SourceLocation(buffer1->id, 0), nullptr, false,
                                      {});
    CHECK(buffer2->data.data() == buffer1->data.data());

    // should be able to load relative
    buffer2 = manager.readHeader("nested/file.svh", SourceLocation(buffer1->id, 0), nullptr, false,
                                 {});
    REQUIRE(buffer2);
    CHECK(!buffer2->data.empty());

    // load another level of relative
    CHECK(
        manager.readHeader("nested_local.svh", SourceLocation(buffer2->id, 0), nullptr, false, {}));
}

TEST_CASE("Read header (include dirs)") {
    SourceManager manager;
    CHECK(!manager.addSystemDirectories(findTestDir()));

    auto buffer = manager.readHeader("include.svh", SourceLocation(), nullptr, true, {});
    REQUIRE(buffer);

    CHECK(!manager.addUserDirectories(findTestDir() + "/nested"));
    buffer = manager.readHeader("../infinite_chain.svh", SourceLocation(buffer->id, 0), nullptr,
                                false, {});
    CHECK(buffer);
}

TEST_CASE("Read header (dev/null)") {
    if (fs::exists("/dev/null")) {
        SourceManager manager;
        auto buffer = manager.readHeader("/dev/null", SourceLocation(), nullptr, true, {});
        CHECK(buffer);
    }
}

static void globAndCheck(const fs::path& basePath, std::string_view pattern, GlobMode mode,
                         GlobRank expectedRank, std::error_code expectedEc,
                         std::initializer_list<const char*> expected) {
    SmallVector<fs::path> results;
    std::error_code ec;
    auto rank = svGlob(basePath, pattern, mode, results, /* expandEnvVars */ true, ec);

    CHECK(rank == expectedRank);
    CHECK(results.size() == expected.size());

    if (ec != expectedEc && ec.default_error_condition() != expectedEc)
        FAIL_CHECK(ec.message() << " != " << expectedEc.message());

    for (auto str : expected) {
        auto it = std::ranges::find_if(results, [str, mode](auto& item) {
            return item.has_filename() ? item.filename() == str
                                       : item.parent_path().filename() == str;
        });
        if (it == results.end()) {
            FAIL_CHECK(str << " is not found in results for " << pattern);
        }
    }

    for (auto& path : results) {
        if (mode == GlobMode::Files)
            CHECK(fs::is_regular_file(path));
        else
            CHECK(fs::is_directory(path));
    }
}

TEST_CASE("File globbing") {
    auto testDir = findTestDir();
    globAndCheck(testDir, "*st?.sv", GlobMode::Files, GlobRank::WildcardName, {},
                 {"test2.sv", "test3.sv", "test4.sv", "test5.sv", "test6.sv", "test7.sv"});
    globAndCheck(testDir, "system", GlobMode::Files, GlobRank::ExactPath,
                 make_error_code(std::errc::is_a_directory), {});
    globAndCheck(testDir, "system/", GlobMode::Files, GlobRank::Directory, {},
                 {"system.svh", "system.map"});
    globAndCheck(testDir, ".../f*.svh", GlobMode::Files, GlobRank::WildcardName, {},
                 {"file.svh", "file_defn.svh", "file_uses_defn.svh"});
    globAndCheck(testDir, "*ste*/", GlobMode::Files, GlobRank::Directory, {},
                 {"file.svh", "macro.svh", "nested_local.svh", "system.svh", "system.map",
                  "incdir_shadow.svh"});
    globAndCheck(testDir, testDir + "/library/pkg.sv", GlobMode::Files, GlobRank::ExactPath, {},
                 {"pkg.sv"});
    globAndCheck(testDir, testDir + "/li?ra?y/pkg.sv", GlobMode::Files, GlobRank::SimpleName, {},
                 {"pkg.sv"});
    globAndCheck(testDir, testDir + ".../pkg.sv", GlobMode::Files, GlobRank::SimpleName, {},
                 {"pkg.sv"});
    globAndCheck(testDir, "*??blah", GlobMode::Files, GlobRank::WildcardName, {}, {});
    globAndCheck(testDir, "blah", GlobMode::Files, GlobRank::ExactPath,
                 make_error_code(std::errc::no_such_file_or_directory), {});

    putenv((char*)"BAR#=cmd");
    globAndCheck(testDir, "*${BAR#}.f", GlobMode::Files, GlobRank::WildcardName, {}, {"cmd.f"});
}

TEST_CASE("Directory globbing") {
    auto testDir = findTestDir();
    globAndCheck(testDir, "*st?.sv", GlobMode::Directories, GlobRank::WildcardName, {}, {});
    globAndCheck(testDir, "system", GlobMode::Directories, GlobRank::ExactPath, {}, {"system"});
    globAndCheck(testDir, "system/", GlobMode::Directories, GlobRank::ExactPath, {}, {"system"});
    globAndCheck(testDir, ".../", GlobMode::Directories, GlobRank::Directory, {},
                 {"library", "nested", "system", "data", "libtest", "dirprefix", "dir_a",
                  "waivers"});
    globAndCheck(testDir, testDir + "/library/pkg.sv", GlobMode::Directories, GlobRank::ExactPath,
                 make_error_code(std::errc::not_a_directory), {});
}

TEST_CASE("File glob infinite recursion") {
    std::error_code ec;
    auto p = fs::temp_directory_path(ec);
    fs::current_path(p, ec);
    fs::create_directories("sandbox/a/b/c", ec);
    fs::create_directories("sandbox/a/b/d/e", ec);
    std::ofstream("sandbox/a/b/file1.txt");
    fs::create_directory_symlink(fs::absolute("sandbox/a", ec), "sandbox/a/b/c/syma", ec);

    globAndCheck({}, "sandbox/...", GlobMode::Files, GlobRank::Directory, {}, {"file1.txt"});

    fs::remove_all("sandbox", ec);
}

TEST_CASE("In-memory glob matching") {
    CHECK(svGlobMatches("foo/bar/baz.txt", "foo/bar/*.txt"));
    CHECK(svGlobMatches("foo/bar/baz.txt", "foo/bar/"));
    CHECK(!svGlobMatches("foo/bar/baz.txt", "foo/bar/*.dat"));
    CHECK(!svGlobMatches("foo/bar/baz.txt", "foo/...bat.txt"));
    CHECK(svGlobMatches("foo/bar/baz.txt", "...baz.txt"));
    CHECK(svGlobMatches("../../foo/bar/baz.txt", "...bar/..."));
    CHECK(svGlobMatches("../../foo/bar/baz.txt", ".../bar/..."));
}

TEST_CASE("Display column with UTF-8") {
    SourceManager manager;

    // Test with UTF-8 characters: "H©llo" (© is 2 bytes, 1 column width)
    std::string utf8Text = "H\xC2\xA9llo";
    auto buffer = manager.assignText("test.sv", utf8Text);
    REQUIRE(buffer);

    // Test locations in the UTF-8 text
    SourceLocation loc1(buffer.id, 0); // 'H' at byte 0
    CHECK(manager.getDisplayColumnNumber(loc1) == 1);

    SourceLocation loc2(buffer.id, 1); // '©' at byte 1 (starts 2-byte sequence)
    CHECK(manager.getDisplayColumnNumber(loc2) == 2);

    SourceLocation loc3(buffer.id, 3); // First 'l' at byte 3 (after 2-byte ©)
    // Since © takes 2 bytes but 1 display column, this should be at display column 3, not 4
    CHECK(manager.getDisplayColumnNumber(loc3) == 3);
}

TEST_CASE("Display column with tabs") {
    SourceManager manager;

    // Test with tab character: "a\tb"
    std::string tabText = "a\tb";
    auto buffer = manager.assignText("test.sv", tabText);
    REQUIRE(buffer);

    // Test specific locations
    SourceLocation loc1(buffer.id, 0); // 'a' at byte 0
    CHECK(manager.getDisplayColumnNumber(loc1) == 1);

    SourceLocation loc2(buffer.id, 1); // '\t' at byte 1
    CHECK(manager.getDisplayColumnNumber(loc2) == 2);

    SourceLocation loc3(buffer.id, 2); // 'b' at byte 2 (after tab)
    // Tab at position 2 expands to next 8-boundary, which is column 9
    CHECK(manager.getDisplayColumnNumber(loc3) == 9);
}

TEST_CASE("Source location lookup by line column") {
    SourceManager manager;
    std::string_view text = "alpha\nbeta\ngamma";
    auto buffer = manager.assignText("lookup.sv", text);
    REQUIRE(buffer);

    for (size_t offset = 0; offset <= text.size(); offset++) {
        SourceLocation fromOffset(buffer.id, offset);
        auto fromLineCol = manager.getSourceLocation(buffer.id, manager.getLineNumber(fromOffset),
                                                     manager.getColumnNumber(fromOffset));
        REQUIRE(fromLineCol);
        CHECK(*fromLineCol == fromOffset);
    }

    auto loc = manager.getSourceLocation(buffer.id, 2, 3);
    REQUIRE(loc);
    CHECK(*loc == SourceLocation(buffer.id, 8));
    CHECK(manager.getLineNumber(*loc) == 2);
    CHECK(manager.getColumnNumber(*loc) == 3);

    loc = manager.getSourceLocation(buffer.id, 1, 6);
    REQUIRE(loc);
    CHECK(*loc == SourceLocation(buffer.id, 5));
    CHECK(manager.getLineNumber(*loc) == 1);
    CHECK(manager.getColumnNumber(*loc) == 6);

    loc = manager.getSourceLocation(buffer.id, 3, 6);
    REQUIRE(loc);
    CHECK(*loc == SourceLocation(buffer.id, text.size()));

    CHECK(!manager.getSourceLocation(buffer.id, 1, 7));
    CHECK(!manager.getSourceLocation(buffer.id, 3, 7));
    CHECK(!manager.getSourceLocation(buffer.id, 0, 1));
    CHECK(!manager.getSourceLocation(buffer.id, 4, 1));
    CHECK(!manager.getSourceLocation(BufferID::getPlaceholder(), 1, 1));
}

TEST_CASE("Source text helpers") {
    SourceManager manager;
    auto buffer = manager.assignText("lines.sv", "first\nsecond\nthird");
    REQUIRE(buffer);

    auto start = manager.getSourceLocation(buffer.id, 2, 1);
    auto end = manager.getSourceLocation(buffer.id, 2, 7);
    REQUIRE(start);
    REQUIRE(end);

    CHECK(manager.getSourceText(SourceRange(*start, *end)) == "second");
    CHECK(manager.getSourceText(SourceRange(*end, *start)).empty());
    CHECK(manager.getSourceText(SourceRange(*start, SourceLocation(buffer.id, 100))).empty());
    CHECK(manager.getSourceText(SourceRange(*start, SourceLocation(BufferID::getPlaceholder(), 0)))
              .empty());
}

TEST_CASE("Compute line offsets from string view") {
    std::vector<size_t> offsets;
    SourceManager::computeLineOffsets("a\nb\r\nc\rd", offsets);

    CHECK(offsets == std::vector<size_t>{0, 2, 5, 7});
}

TEST_CASE("Fully expanded range follows regular and argument macro expansions") {
    SourceManager manager;
    auto defBuffer = manager.assignText("defs.svh", "`define M(arg) $info(arg, arg)\n");
    auto useBuffer = manager.assignText("use.sv", "`M(1 + 2)\n");
    REQUIRE(defBuffer);
    REQUIRE(useBuffer);

    SourceRange usageRange(SourceLocation(useBuffer.id, 0), SourceLocation(useBuffer.id, 9));
    auto bodyLoc = manager.createExpansionLoc(SourceLocation(defBuffer.id, 15), usageRange, "M");

    CHECK(manager.getFullyExpandedRange(SourceRange(bodyLoc, bodyLoc + 5)).start() ==
          usageRange.start());
    CHECK(manager.getFullyExpandedRange(SourceRange(bodyLoc, bodyLoc + 5)).end() ==
          usageRange.end());

    SourceRange firstArgUseInExpansion(bodyLoc + 6, bodyLoc + 9);
    auto argLoc = manager.createArgExpansionLoc(SourceLocation(useBuffer.id, 3),
                                                firstArgUseInExpansion);
    auto expandedRange = manager.getFullyExpandedRange(SourceRange(argLoc, argLoc + 5));

    CHECK(expandedRange.start() == usageRange.start());
    CHECK(expandedRange.end() == usageRange.end());
}
