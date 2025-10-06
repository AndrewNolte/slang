// SPDX-FileCopyrightText: Michael Popoloski
// SPDX-License-Identifier: MIT

#include "Test.h"
#include "catch2/catch_test_macros.hpp"

#include "slang/syntax/AllSyntax.h"
#include "slang/syntax/SyntaxPrinter.h"

TEST_CASE("Print leading comments - line comments") {
    auto& cu = parseCompilationUnit(R"(
// This is a leading comment
// for the module
module m;
endmodule
)");

    auto& module = cu.members[0]->as<ModuleDeclarationSyntax>();
    std::string result = SyntaxPrinter().printLeadingComments(module).str();
    CHECK(result == "// This is a leading comment\n// for the module\n");
}

TEST_CASE("Print leading comments - spaced out line comments") {
    auto& cu = parseCompilationUnit(R"(
// This is a also a leading comment
// for the module

module m;
endmodule
)");

    auto& module = cu.members[0]->as<ModuleDeclarationSyntax>();
    std::string result = SyntaxPrinter().printLeadingComments(module).str();
    CHECK(result == "// This is a also a leading comment\n// for the module\n");
}

TEST_CASE("Print leading comments - block comment stops at newline") {
    auto& cu = parseCompilationUnit(R"(
/* Not included */
/* Block comment */
module m;
endmodule
)");

    auto& module = cu.members[0]->as<ModuleDeclarationSyntax>();
    std::string result = SyntaxPrinter().printLeadingComments(module).str();
    CHECK(result == "/* Block comment */\n");
}

TEST_CASE("Print leading comments - double newline boundary") {
    auto& cu = parseCompilationUnit(R"(
// Not a leading comment

// This is a leading comment
module m;
endmodule
)");

    auto& module = cu.members[0]->as<ModuleDeclarationSyntax>();
    std::string result = SyntaxPrinter().printLeadingComments(module).str();
    CHECK(result == "// This is a leading comment\n");
}

TEST_CASE("Print leading comments - previous line comment and leading comment") {
    auto& cu = parseCompilationUnit(R"(
asdf; // Not part of leading comment
// This is a leading comment
module m;
endmodule
)");

    REQUIRE(cu.members.size() >= 2);
    auto& module = cu.members[1]->as<ModuleDeclarationSyntax>();
    std::string result = SyntaxPrinter().printLeadingComments(module).str();
    CHECK(result == "// This is a leading comment\n");
}

TEST_CASE("Print leading comments - previous line comment") {
    auto& cu = parseCompilationUnit(R"(
    asdf; // Not part of leading comment
    module m;
    endmodule
)");

    REQUIRE(cu.members.size() >= 2);
    auto& module = cu.members[1]->as<ModuleDeclarationSyntax>();
    std::string result = SyntaxPrinter().printLeadingComments(module).str();
    CHECK(result == "");
}

TEST_CASE("Print leading comments - multiple line comments") {
    auto& cu = parseCompilationUnit(R"(
// Comment line 1
// Comment line 2
// Comment line 3
module m;
endmodule
)");

    auto& module = cu.members[0]->as<ModuleDeclarationSyntax>();
    std::string result = SyntaxPrinter().printLeadingComments(module).str();
    CHECK(result == "// Comment line 1\n// Comment line 2\n// Comment line 3\n");
}

TEST_CASE("Print leading comments - no comments") {
    auto& cu = parseCompilationUnit(R"(
module m;
endmodule
)");

    auto& module = cu.members[0]->as<ModuleDeclarationSyntax>();
    std::string result = SyntaxPrinter().printLeadingComments(module).str();
    CHECK(result == "");
}

TEST_CASE("Print with leading comments") {
    auto& cu = parseCompilationUnit(R"(
// This is a leading comment
module m;
endmodule
)");

    auto& module = cu.members[0]->as<ModuleDeclarationSyntax>();
    std::string result =
        SyntaxPrinter().setIncludeTrivia(false).printWithLeadingComments(module).str();
    // printWithLeadingComments modifies includeTrivia; so check that it resets it back
    CHECK(result == "// This is a leading comment\nmodulem;endmodule");
}

TEST_CASE("Print leading comments - indented leading comments") {
    auto& cu = parseCompilationUnit(R"(
module top;
    // Internal comment
    reg x;
endmodule
)");

    auto& module = cu.members[0]->as<ModuleDeclarationSyntax>();
    auto& body = module.as<ModuleDeclarationSyntax>();
    REQUIRE(body.members.size() > 0);
    auto& reg = *body.members[0];

    std::string result = SyntaxPrinter().printLeadingComments(reg).str();
    // Should not include just whitespace/tabs before the comment
    CHECK(result == R"(// Internal comment
    )");
}

TEST_CASE("Print leading comments - indented blank leading comment") {
    auto& cu = parseCompilationUnit(R"(
module top;


    reg x;
endmodule
)");

    auto& module = cu.members[0]->as<ModuleDeclarationSyntax>();
    auto& body = module.as<ModuleDeclarationSyntax>();
    REQUIRE(body.members.size() > 0);
    auto& reg = *body.members[0];

    std::string result = SyntaxPrinter().printLeadingComments(reg).str();
    // Empty lines (whitespace only) should not be considered leading comments
    CHECK(result == "");
}
