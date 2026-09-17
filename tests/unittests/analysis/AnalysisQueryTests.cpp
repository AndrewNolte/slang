// SPDX-FileCopyrightText: Michael Popoloski
// SPDX-License-Identifier: MIT

#include "AnalysisTests.h"

#include "slang/analysis/AnalysisQueries.h"
#include "slang/analysis/ValueDriver.h"

TEST_CASE("Driver queries support non-canonical procedural locals") {
    auto& code = R"(
module leaf;
    logic result;

    function logic calculate;
        logic functionLocal;
        functionLocal = 1'b1;
        return functionLocal;
    endfunction

    always_comb begin : block
        logic blockLocal;
        blockLocal = 1'b1;
        result = calculate() & blockLocal;
    end
endmodule

module top;
    leaf first();
    leaf second();
endmodule
)";

    Compilation compilation;
    AnalysisManager analysisManager;
    auto diags = analyze(code, compilation, analysisManager);
    CHECK_DIAGS_EMPTY;
    compilation.unfreeze();
    AnalysisQueries queries(compilation, analysisManager);

    auto& root = compilation.getRoot();
    auto& second = root.lookupName<InstanceSymbol>("top.second");
    REQUIRE(second.getCanonicalBody());

    auto& blockLocal = root.lookupName<VariableSymbol>("top.second.block.blockLocal");
    auto blockDrivers = queries.getDrivers(blockLocal);
    REQUIRE(blockDrivers.size() == 1);
    CHECK(&blockDrivers[0]->getSymbol() == &blockLocal);

    auto& calculate = second.body.find<SubroutineSymbol>("calculate");
    auto& functionLocal = calculate.find<VariableSymbol>("functionLocal");
    auto& first = root.lookupName<InstanceSymbol>("top.first");
    auto& canonicalCalculate = first.body.find<SubroutineSymbol>("calculate");
    auto& canonicalFunctionLocal = canonicalCalculate.find<VariableSymbol>("functionLocal");
    REQUIRE(queries.getDrivers(canonicalFunctionLocal).size() == 2);
    auto functionDrivers = queries.getDrivers(functionLocal);
    REQUIRE(functionDrivers.size() == 2);
    for (auto* driver : functionDrivers)
        CHECK(&driver->getSymbol() == &functionLocal);
}

TEST_CASE("Driver queries map containing symbols across non-canonical hierarchies") {
    auto& code = R"(
module nested;
    logic value;
    logic externalValue;
endmodule

module driven;
    logic value;
    always_comb value = 1'b1;
endmodule

module container;
    nested child();
    driven firstDriven();
    driven secondDriven();
    always_comb child.value = 1'b1;
endmodule

module top;
    container first();
    container second();
    always_comb first.child.externalValue = 1'b1;
endmodule
)";

    Compilation compilation;
    AnalysisManager analysisManager;
    auto diags = analyze(code, compilation, analysisManager);
    CHECK_DIAGS_EMPTY;
    compilation.unfreeze();
    AnalysisQueries queries(compilation, analysisManager);

    auto& root = compilation.getRoot();
    auto& second = root.lookupName<InstanceSymbol>("top.second");
    REQUIRE(second.getCanonicalBody());

    auto& value = root.lookupName<VariableSymbol>("top.second.child.value");
    auto drivers = queries.getDrivers(value);
    REQUIRE(drivers.size() == 1);
    CHECK(&drivers[0]->getSymbol() == &value);
    CHECK(drivers[0]->containingSymbol->getParentScope() == &second.body);

    auto& externalValue = root.lookupName<VariableSymbol>("top.second.child.externalValue");
    CHECK(queries.getDrivers(externalValue).empty());

    auto& secondDriven = root.lookupName<InstanceSymbol>("top.second.secondDriven");
    auto& drivenValue = root.lookupName<VariableSymbol>("top.second.secondDriven.value");
    auto drivenDrivers = queries.getDrivers(drivenValue);
    REQUIRE(drivenDrivers.size() == 1);
    CHECK(&drivenDrivers[0]->getSymbol() == &drivenValue);
    CHECK(drivenDrivers[0]->containingSymbol->getParentScope() == &secondDriven.body);
}

TEST_CASE("Driver queries preserve non-canonical ref port side effects") {
    auto& code = R"(
module writer(ref logic target);
    always_comb target = 1'b1;
endmodule

module container;
    logic firstValue;
    logic secondValue;
    writer firstChild(firstValue);
    writer secondChild(secondValue);
endmodule

module top;
    container first();
    container second();
endmodule
)";

    Compilation compilation;
    AnalysisManager analysisManager;
    auto diags = analyze(code, compilation, analysisManager);
    CHECK_DIAGS_EMPTY;
    compilation.unfreeze();
    AnalysisQueries queries(compilation, analysisManager);

    auto& root = compilation.getRoot();
    auto& second = root.lookupName<InstanceSymbol>("top.second");
    REQUIRE(second.getCanonicalBody());

    auto& firstChild = root.lookupName<InstanceSymbol>("top.second.firstChild");
    auto& firstValue = root.lookupName<VariableSymbol>("top.second.firstValue");
    auto firstDrivers = queries.getDrivers(firstValue);
    REQUIRE(firstDrivers.size() == 1);
    CHECK(&firstDrivers[0]->getSymbol() == &firstValue);
    CHECK(firstDrivers[0]->flags.has(DriverFlags::ViaIndirectPort));
    CHECK(firstDrivers[0]->containingSymbol->getParentScope() == &firstChild.body);

    auto& secondChild = root.lookupName<InstanceSymbol>("top.second.secondChild");
    auto& secondValue = root.lookupName<VariableSymbol>("top.second.secondValue");
    auto secondDrivers = queries.getDrivers(secondValue);
    REQUIRE(secondDrivers.size() == 1);
    CHECK(&secondDrivers[0]->getSymbol() == &secondValue);
    CHECK(secondDrivers[0]->flags.has(DriverFlags::FromSideEffect));
    CHECK(secondDrivers[0]->containingSymbol == &secondChild);
}

TEST_CASE("Driver queries do not add diagnostics") {
    auto& code = R"(
module leaf;
    logic value;
    always_comb value = 1'b0;
    always_comb value = 1'b1;
endmodule

module top;
    leaf first();
    leaf second();
endmodule
)";

    Compilation compilation;
    AnalysisManager analysisManager;
    auto diags = analyze(code, compilation, analysisManager);
    REQUIRE(diags.size() == 1);
    CHECK(diags[0].code == diag::MultipleAlwaysAssigns);
    compilation.unfreeze();
    AnalysisQueries queries(compilation, analysisManager);

    auto& root = compilation.getRoot();
    auto& value = root.lookupName<VariableSymbol>("top.second.value");
    REQUIRE(queries.getDrivers(value).size() == 2);

    auto diagsAfterQuery = analysisManager.getDiagnostics();
    REQUIRE(diagsAfterQuery.size() == 1);
    CHECK(diagsAfterQuery[0].code == diag::MultipleAlwaysAssigns);
}

TEST_CASE("Driver queries separate recorded results from instance localization") {
    auto& code = R"(
module leaf;
    logic value;
    always_comb value = 1'b0;
endmodule
module top;
    leaf first();
    leaf second();
endmodule
)";

    Compilation compilation;
    AnalysisManager analysisManager;
    compilation.addSyntaxTree(SyntaxTree::fromText(code));
    NO_COMPILATION_ERRORS;

    auto& root = compilation.getRoot();
    REQUIRE(root.lookupName<InstanceSymbol>("top.second").getCanonicalBody());
    auto& value = root.lookupName<VariableSymbol>("top.second.value");
    auto& canonicalValue = root.lookupName<VariableSymbol>("top.first.value");
    compilation.freeze();
    analysisManager.analyze(compilation);

    CHECK(analysisManager.getDrivers(canonicalValue).size() == 1);
    CHECK(analysisManager.getDrivers(value).empty());
#if __cpp_exceptions && SLANG_ASSERT_ENABLED
    CHECK_THROWS_AS(AnalysisQueries(compilation, analysisManager),
                    slang::assert::AssertionException);
#endif

    compilation.unfreeze();
    AnalysisQueries queries(compilation, analysisManager);
    auto drivers = queries.getDrivers(value);
    REQUIRE(drivers.size() == 1);
    CHECK(&drivers[0]->getSymbol() == &value);
    CHECK(queries.getDrivers(value) == drivers);
    CHECK(analysisManager.getDrivers(value).empty());

#if __cpp_exceptions && SLANG_ASSERT_ENABLED
    compilation.freeze();
    CHECK_THROWS_AS(queries.getDrivers(value), slang::assert::AssertionException);
    CHECK(compilation.isFrozen());
    compilation.unfreeze();
#endif
}
