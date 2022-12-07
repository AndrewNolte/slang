//------------------------------------------------------------------------------
// ParameterBuilder.h
// Helper for constructing parameter symbols
//
// SPDX-FileCopyrightText: Michael Popoloski
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------
#pragma once

#include "slang/ast/Definition.h"
#include "slang/util/StackContainer.h"

namespace slang::ast {

class ParameterSymbolBase;
class Scope;
class Type;
struct ParamOverrideNode;

/// This is a helper type for turning parameter-related syntax nodes into actual
/// parameter symbols and applying values to them. The logic here is factored out
/// so that it can be shared by both module/interface definitions as well as
/// generic class definitions.
class ParameterBuilder {
public:
    using Decl = Definition::ParameterDecl;

    ParameterBuilder(const Scope& scope, string_view definitionName,
                     span<const Decl> parameterDecls);

    bool hasErrors() const { return anyErrors; }

    void setAssignments(const syntax::ParameterValueAssignmentSyntax& syntax);
    void setOverrides(const ParamOverrideNode* newVal) { overrideNode = newVal; }
    void setForceInvalidValues(bool set) { forceInvalidValues = set; }
    void setSuppressErrors(bool set) { suppressErrors = set; }
    void setInstanceContext(const ASTContext& context) { instanceContext = &context; }

    const ParamOverrideNode* getOverrides() const { return overrideNode; }

    const ParameterSymbolBase& createParam(const Definition::ParameterDecl& decl, Scope& newScope,
                                           SourceLocation instanceLoc);

    static void createDecls(const Scope& scope,
                            const syntax::ParameterDeclarationBaseSyntax& syntax, bool isLocal,
                            bool isPort,
                            span<const syntax::AttributeInstanceSyntax* const> attributes,
                            SmallVectorBase<Decl>& results);
    static void createDecls(const Scope& scope, const syntax::ParameterPortListSyntax& syntax,
                            SmallVectorBase<Decl>& results);

private:
    const Scope& scope;
    string_view definitionName;
    span<const Decl> parameterDecls;
    SmallMap<string_view, const syntax::ExpressionSyntax*, 8> assignments;
    const ASTContext* instanceContext = nullptr;
    const ParamOverrideNode* overrideNode = nullptr;
    bool forceInvalidValues = false;
    bool suppressErrors = false;
    bool anyErrors = false;
};

} // namespace slang::ast
