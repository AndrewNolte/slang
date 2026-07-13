//------------------------------------------------------------------------------
// ParameterBuilder.h
// Helper for constructing parameter symbols
//
// SPDX-FileCopyrightText: Michael Popoloski
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------
#pragma once

#include <variant>

#include "slang/ast/symbols/CompilationUnitSymbols.h"
#include "slang/numeric/ConstantValue.h"
#include "slang/util/SmallMap.h"

namespace slang::ast {

class ParameterSymbolBase;
class Scope;
class Type;
struct HierarchyOverrideNode;

/// This is a helper type for turning parameter-related syntax nodes into actual
/// parameter symbols and applying values to them. The logic here is factored out
/// so that it can be shared by both module/interface definitions as well as
/// generic class definitions.
class ParameterBuilder {
public:
    using Decl = DefinitionSymbol::ParameterDecl;

    ParameterBuilder(const Scope& scope, std::string_view definitionName,
                     std::span<const Decl> parameterDecls);

    bool hasErrors() const { return anyErrors; }

    void setAssignments(const syntax::ParameterValueAssignmentSyntax& syntax, bool isFromConfig);

    /// Adds a pre-resolved override for a value parameter. Unlike a syntactic assignment, the
    /// value is already evaluated, so no instance context is needed and it is applied directly.
    /// Useful for propagating an already-elaborated value (e.g. copied from another instance, or
    /// derived from a constraint) onto the parameter of a freshly built instance.
    void addValueOverride(std::string_view name, ConstantValue value) {
        resolvedOverrides.emplace(name, std::move(value));
    }

    /// Adds a pre-resolved override for a type parameter. As with @a addValueOverride the type is
    /// already resolved, so no instance context is needed.
    void addTypeOverride(std::string_view name, const Type& type) {
        resolvedOverrides.emplace(name, &type);
    }

    void setOverrides(const HierarchyOverrideNode* newVal) { overrideNode = newVal; }
    /// Force invalid values (error type) for all parameters.
    void setForceInvalidValues(bool set) { forceInvalidValues = set; }
    /// Set invalid values (error type) for parameters that are missing values, rather than
    /// reporting errors.
    void setUseInvalidForMissing(bool set) { useInvalidForMissing = set; }
    /// Suppress error reporting for missing parameter values.
    void setSuppressErrors(bool set) { suppressErrors = set; }
    void setInstanceContext(const ASTContext& context) { instanceContext = &context; }
    void setConfigScope(const Scope& confScope) { configScope = &confScope; }

    const HierarchyOverrideNode* getOverrides() const { return overrideNode; }

    const ParameterSymbolBase& createParam(const DefinitionSymbol::ParameterDecl& decl,
                                           Scope& newScope, SourceLocation instanceLoc);

    static void createDecls(const Scope& scope,
                            const syntax::ParameterDeclarationBaseSyntax& syntax, bool isLocal,
                            bool isPort,
                            std::span<const syntax::AttributeInstanceSyntax* const> attributes,
                            SmallVectorBase<Decl>& results);
    static void createDecls(const Scope& scope, const syntax::ParameterPortListSyntax& syntax,
                            SmallVectorBase<Decl>& results);

private:
    const Scope& scope;
    std::string_view definitionName;
    std::span<const Decl> parameterDecls;
    SmallMap<std::string_view, std::pair<const syntax::ExpressionSyntax*, bool>, 8> assignments;

    // Pre-resolved overrides keyed by parameter name: a ConstantValue for value parameters or a
    // Type for type parameters. Applied directly, without needing an instance context.
    SmallMap<std::string_view, std::variant<ConstantValue, const Type*>, 2> resolvedOverrides;

    const ASTContext* instanceContext = nullptr;
    const HierarchyOverrideNode* overrideNode = nullptr;
    const Scope* configScope = nullptr;
    bool forceInvalidValues = false;
    bool useInvalidForMissing = false;
    bool suppressErrors = false;
    bool anyErrors = false;
};

} // namespace slang::ast
