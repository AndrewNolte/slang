//------------------------------------------------------------------------------
//! @file SourceManager.h
//! @brief Source file management
//
// File is under the MIT license; see LICENSE for details
//------------------------------------------------------------------------------
#pragma once

#include <atomic>
#include <deque>
#include <filesystem>
#include <flat_hash_map.hpp>
#include <memory>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <unordered_map>
#include <variant>
#include <vector>

#include "slang/text/SourceLocation.h"
#include "slang/util/Util.h"

namespace fs = std::filesystem;

namespace slang {

enum class DiagnosticSeverity;

/// SourceManager - Handles loading and tracking source files.
///
/// The source manager abstracts away the differences between
/// locations in files and locations generated by macro expansion.
/// See SourceLocation for more details.
///
/// The methods in this class are thread safe.
class SourceManager {
public:
    SourceManager();
    SourceManager(const SourceManager&) = delete;
    SourceManager& operator=(const SourceManager&) = delete;

    /// Convert the given relative path into an absolute path.
    std::string makeAbsolutePath(string_view path) const;

    /// Adds a system include directory.
    void addSystemDirectory(string_view path);

    /// Adds a user include directory.
    void addUserDirectory(string_view path);

    /// Gets the source line number for a given source location.
    size_t getLineNumber(SourceLocation location) const;

    /// Gets the source file name for a given source location.
    string_view getFileName(SourceLocation location) const;

    /// Gets the source file name for a given source buffer, not taking
    /// into account any `line directives that may be in the file.
    string_view getRawFileName(BufferID buffer) const;

    /// Gets the column line number for a given source location.
    /// @a location must be a file location.
    size_t getColumnNumber(SourceLocation location) const;

    /// Gets a location that indicates from where the given buffer was included.
    /// @a location must be a file location.
    SourceLocation getIncludedFrom(BufferID buffer) const;

    /// Attempts to get the name of the macro represented by a macro location.
    /// If no macro name can be found, returns an empty string view.
    string_view getMacroName(SourceLocation location) const;

    /// Determines whether the given location exists in a source file.
    bool isFileLoc(SourceLocation location) const;

    /// Determines whether the given location points to a macro expansion.
    bool isMacroLoc(SourceLocation location) const;

    /// Determines whether the given location points to a macro argument expansion.
    bool isMacroArgLoc(SourceLocation location) const;

    /// Determines whether the given location is inside an include file.
    bool isIncludedFileLoc(SourceLocation location) const;

    /// Determines whether the given location is from a macro expansion or an include file.
    bool isPreprocessedLoc(SourceLocation location) const;

    /// Determines whether the @param left location comes before the @param right location
    /// within the "compilation unit space", which is a hypothetical source space where
    /// all macros and include files have been expanded out into a flat file.
    bool isBeforeInCompilationUnit(SourceLocation left, SourceLocation right) const;

    /// Gets the expansion location of a given macro location.
    SourceLocation getExpansionLoc(SourceLocation location) const;

    /// Gets the expansion range of a given macro location.
    SourceRange getExpansionRange(SourceLocation location) const;

    /// Gets the original source location of a given macro location.
    SourceLocation getOriginalLoc(SourceLocation location) const;

    /// Gets the actual original location where source is written, given a location
    /// inside a macro. Otherwise just returns the location itself.
    SourceLocation getFullyOriginalLoc(SourceLocation location) const;

    /// If the given location is a macro location, fully expands it out to its actual
    /// file expansion location. Otherwise just returns the location itself.
    SourceLocation getFullyExpandedLoc(SourceLocation location) const;

    /// Gets the actual source text for a given file buffer.
    string_view getSourceText(BufferID buffer) const;

    /// Creates a macro expansion location; used by the preprocessor.
    SourceLocation createExpansionLoc(SourceLocation originalLoc, SourceRange expansionRange,
                                      bool isMacroArg);

    /// Creates a macro expansion location; used by the preprocessor.
    SourceLocation createExpansionLoc(SourceLocation originalLoc, SourceRange expansionRange,
                                      string_view macroName);

    /// Instead of loading source from a file, copy it from text already in memory.
    SourceBuffer assignText(string_view text, SourceLocation includedFrom = SourceLocation());

    /// Instead of loading source from a file, copy it from text already in memory.
    /// Pretend it came from a file located at @a path.
    SourceBuffer assignText(string_view path, string_view text,
                            SourceLocation includedFrom = SourceLocation());

    /// Instead of loading source from a file, move it from text already in memory.
    /// Pretend it came from a file located at @a path.
    SourceBuffer assignBuffer(string_view path, std::vector<char>&& buffer,
                              SourceLocation includedFrom = SourceLocation());

    /// Read in a source file from disk.
    SourceBuffer readSource(const fs::path& path);

    /// Read in a header file from disk.
    SourceBuffer readHeader(string_view path, SourceLocation includedFrom, bool isSystemPath);

    /// Returns true if the given file path is already loaded and cached in the source manager.
    bool isCached(const fs::path& path) const;

    /// Sets whether filenames should be made "proximate" to the current directory
    /// for diagnostic reporting purposes. This is on by default but can be
    /// disabled to always use the simple filename.
    void setDisableProximatePaths(bool set) { disableProximatePaths = set; }

    /// Adds a line directive at the given location.
    void addLineDirective(SourceLocation location, size_t lineNum, string_view name, uint8_t level);

    /// Adds a diagnostic directive at the given location.
    void addDiagnosticDirective(SourceLocation location, string_view name,
                                DiagnosticSeverity severity);

    /// Stores information specified in a `pragma diagnostic directive, which alters the
    /// currently active set of diagnostic mappings.
    struct DiagnosticDirectiveInfo {
        /// The name of the diagnostic being controlled.
        string_view name;

        /// Offset in the source where the directive occurred.
        size_t offset;

        /// The new severity the diagnostic should have.
        DiagnosticSeverity severity;

        DiagnosticDirectiveInfo(string_view name, size_t offset,
                                DiagnosticSeverity severity) noexcept :
            name(name),
            offset(offset), severity(severity) {}
    };

    /// Visits each buffer that contains diagnostic directives and invokes the provided
    /// callback with the first argument being the buffer and the second being an
    /// iterable collection of DiagnosticDirectiveInfos.
    template<typename Func>
    void visitDiagnosticDirectives(Func&& func) const {
        for (auto& [buffer, directives] : diagDirectives)
            func(buffer, directives);
    }

private:
    // Stores information specified in a `line directive, which alters the
    // line number and file name that we report in diagnostics.
    struct LineDirectiveInfo {
        std::string name;       // File name set by directive
        size_t lineInFile;      // Actual file line where directive occurred
        size_t lineOfDirective; // Line number set by directive
        uint8_t level;          // Level of directive. Either 0, 1, or 2.

        LineDirectiveInfo(std::string&& fname, size_t lif, size_t lod, uint8_t level) noexcept :
            name(std::move(fname)), lineInFile(lif), lineOfDirective(lod), level(level) {}
    };

    // Stores actual file contents and metadata; only one per loaded file
    struct FileData {
        const std::string name;          // name of the file
        const std::vector<char> mem;     // file contents
        std::vector<size_t> lineOffsets; // cache of compute line offsets
        const fs::path* const directory; // directory in which the file exists

        FileData(const fs::path* directory, std::string name, std::vector<char>&& data) :
            name(std::move(name)), mem(std::move(data)), directory(directory) {}
    };

    // Stores a pointer to file data along with information about where we included it.
    // There can potentially be many of these for a given file.
    struct FileInfo {
        FileData* data = nullptr;
        SourceLocation includedFrom;
        std::vector<LineDirectiveInfo> lineDirectives;

        FileInfo() {}
        FileInfo(FileData* data, SourceLocation includedFrom) :
            data(data), includedFrom(includedFrom) {}

        // Returns a pointer to the LineDirectiveInfo for the nearest enclosing
        // line directive of the given raw line number, or nullptr if there is none
        const LineDirectiveInfo* getPreviousLineDirective(size_t rawLineNumber) const;
    };

    // Instead of a file, this lets a BufferID point to a macro expansion location.
    // This is actually used two different ways; if this is a normal token from a
    // macro expansion, originalLocation will point to the token inside the macro
    // definition, and expansionLocation will point to the range of the macro usage
    // at the expansion site. Alternatively, if this token came from an argument,
    // originalLocation will point to the argument at the expansion site and
    // expansionLocation will point to the parameter inside the macro body.
    struct ExpansionInfo {
        SourceLocation originalLoc;
        SourceRange expansionRange;
        bool isMacroArg = false;

        string_view macroName;

        ExpansionInfo() {}
        ExpansionInfo(SourceLocation originalLoc, SourceRange expansionRange, bool isMacroArg) :
            originalLoc(originalLoc), expansionRange(expansionRange), isMacroArg(isMacroArg) {}

        ExpansionInfo(SourceLocation originalLoc, SourceRange expansionRange,
                      string_view macroName) :
            originalLoc(originalLoc),
            expansionRange(expansionRange), macroName(macroName) {}
    };

    mutable std::shared_mutex mut;

    // index from BufferID to buffer metadata
    std::deque<std::variant<FileInfo, ExpansionInfo>> bufferEntries;

    // cache for file lookups; this holds on to the actual file data
    std::unordered_map<std::string, std::unique_ptr<FileData>> lookupCache;

    // directories for system and user includes
    std::vector<fs::path> systemDirectories;
    std::vector<fs::path> userDirectories;

    // uniquified backing memory for directories
    std::set<fs::path> directories;

    // map from buffer to diagnostic directive lists
    flat_hash_map<BufferID, std::vector<DiagnosticDirectiveInfo>> diagDirectives;

    std::atomic<uint32_t> unnamedBufferCount = 0;
    bool disableProximatePaths = false;

    FileInfo* getFileInfo(BufferID buffer);
    const FileInfo* getFileInfo(BufferID buffer) const;
    SourceBuffer createBufferEntry(FileData* fd, SourceLocation includedFrom,
                                   std::unique_lock<std::shared_mutex>& lock);

    SourceBuffer openCached(const fs::path& fullPath, SourceLocation includedFrom);
    SourceBuffer cacheBuffer(const fs::path& path, SourceLocation includedFrom,
                             std::vector<char>&& buffer);

    // Get raw line number of a file location, ignoring any line directives
    size_t getRawLineNumber(SourceLocation location) const;

    static void computeLineOffsets(const std::vector<char>& buffer,
                                   std::vector<size_t>& offsets) noexcept;

    static bool readFile(const fs::path& path, std::vector<char>& buffer);
};

} // namespace slang
