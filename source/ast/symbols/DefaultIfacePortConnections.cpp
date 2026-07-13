//------------------------------------------------------------------------------
// DefaultIfacePortConnections.cpp
// Contains support for synthesizing top-level interface port connections
//
// SPDX-FileCopyrightText: Michael Popoloski
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------
#include "ParameterBuilder.h"
#include <optional>
#include <variant>

#include "slang/ast/Compilation.h"
#include "slang/ast/Expression.h"
#include "slang/ast/SemanticFacts.h"
#include "slang/ast/symbols/InstanceSymbols.h"
#include "slang/ast/symbols/PortSymbols.h"
#include "slang/ast/types/Type.h"
#include "slang/syntax/AllSyntax.h"

namespace slang::ast {

// A constraint extracted from a `$static_assert` that pins a parameter of an interface port. The
// `operand` is the other side of the equality; for a value parameter it is the value expression,
// and for a type parameter (`isType`) it is a `type(...)` reference to resolve into the type.
struct IfaceParamConstraint {
    std::string_view name;
    const syntax::ExpressionSyntax* operand;
    bool isType;
};

// Returns the parameter name if `expr` is `portName.<param>`. The access can parse either as a
// member-access expression or, when the left side could be a scope, as a dotted scoped name.
static std::string_view asPortParamAccess(const syntax::ExpressionSyntax& expr,
                                          std::string_view portName) {
    if (expr.kind == syntax::SyntaxKind::MemberAccessExpression) {
        auto& access = expr.as<syntax::MemberAccessExpressionSyntax>();
        if (access.left->kind != syntax::SyntaxKind::IdentifierName)
            return {};
        if (access.left->as<syntax::IdentifierNameSyntax>().identifier.valueText() != portName)
            return {};
        return access.name.valueText();
    }

    if (expr.kind == syntax::SyntaxKind::ScopedName) {
        auto& scoped = expr.as<syntax::ScopedNameSyntax>();
        if (scoped.separator.kind != parsing::TokenKind::Dot)
            return {};
        if (scoped.left->kind != syntax::SyntaxKind::IdentifierName ||
            scoped.right->kind != syntax::SyntaxKind::IdentifierName) {
            return {};
        }
        if (scoped.left->as<syntax::IdentifierNameSyntax>().identifier.valueText() != portName)
            return {};
        return scoped.right->as<syntax::IdentifierNameSyntax>().identifier.valueText();
    }

    return {};
}

// Given a `$static_assert` condition expression, see if it pins a parameter of the given
// interface port to a constant. Two shapes are recognized (with either operand on the left):
//   - value:  `<port>.<param> == <expr>`
//   - type:   `type(<port>.<param>) == type(<expr>)`
// On a match, returns the parameter name, the other operand, and whether it is a type constraint.
static std::optional<IfaceParamConstraint> matchIfaceParamConstraint(
    const syntax::ExpressionSyntax& condition, std::string_view portName) {
    if (condition.kind != syntax::SyntaxKind::EqualityExpression)
        return std::nullopt;

    auto& binExpr = condition.as<syntax::BinaryExpressionSyntax>();
    auto& left = *binExpr.left;
    auto& right = *binExpr.right;

    // Value form: `port.param == <expr>`.
    if (auto param = asPortParamAccess(left, portName); !param.empty())
        return IfaceParamConstraint{param, &right, /* isType */ false};
    if (auto param = asPortParamAccess(right, portName); !param.empty())
        return IfaceParamConstraint{param, &left, /* isType */ false};

    // Type form: `type(port.param) == type(<expr>)`. Both operands are `type(...)` references; the
    // caller resolves the non-port operand (still a `type(...)`) into the override type.
    auto isTypeRef = [](const syntax::ExpressionSyntax& expr) {
        return expr.kind == syntax::SyntaxKind::TypeReference;
    };
    auto typeRefPort = [&](const syntax::ExpressionSyntax& expr) -> std::string_view {
        if (!isTypeRef(expr))
            return {};
        return asPortParamAccess(*expr.as<syntax::TypeReferenceSyntax>().expr, portName);
    };

    if (isTypeRef(left) && isTypeRef(right)) {
        if (auto param = typeRefPort(left); !param.empty())
            return IfaceParamConstraint{param, &right, /* isType */ true};
        if (auto param = typeRefPort(right); !param.empty())
            return IfaceParamConstraint{param, &left, /* isType */ true};
    }

    return std::nullopt;
}

// Scan the body of the instantiating module for `$static_assert` constraints that pin
// parameters of the given default-instantiated interface port to specific values or types,
// collecting them into @a overrides. This lets shallow / top-level interface-port elaboration
// honor constraints like `$static_assert(my_if.PARAM == 2)` even though the port has no real
// connection providing the value.
//
// Only asserts written directly in the module body are considered. Asserts nested in generate
// blocks are skipped: their constraints are conditional, but the override has to be applied
// before elaboration (so before we know which branches are taken), and pinning a parameter from
// a possibly-untaken branch would be wrong.
static void collectIfaceParamConstraints(const InterfacePortSymbol& port,
                                         const syntax::ModuleDeclarationSyntax& topBody,
                                         SmallVectorBase<IfaceParamConstraint>& constraints) {
    auto& def = *port.interfaceDef;

    for (auto member : topBody.members) {
        if (member->kind != syntax::SyntaxKind::ElabSystemTask)
            continue;

        auto& task = member->as<syntax::ElabSystemTaskSyntax>();
        if (SemanticFacts::getElabSystemTaskKind(task.name) != ElabSystemTaskKind::StaticAssert)
            continue;
        if (!task.arguments || task.arguments->parameters.empty())
            continue;

        auto firstArg = task.arguments->parameters[0];
        if (firstArg->kind != syntax::SyntaxKind::OrderedArgument)
            continue;

        // Unwrap the property/sequence wrappers down to a plain expression.
        auto& propExpr = *firstArg->as<syntax::OrderedArgumentSyntax>().expr;
        if (propExpr.kind != syntax::SyntaxKind::SimplePropertyExpr)
            continue;

        auto& seqExpr = *propExpr.as<syntax::SimplePropertyExprSyntax>().expr;
        if (seqExpr.kind != syntax::SyntaxKind::SimpleSequenceExpr)
            continue;

        auto& simpleSeq = seqExpr.as<syntax::SimpleSequenceExprSyntax>();
        if (simpleSeq.repetition)
            continue;

        auto match = matchIfaceParamConstraint(*simpleSeq.expr, port.name);
        if (!match)
            continue;

        // Only override parameters the interface actually declares.
        for (auto& decl : def.parameters) {
            if (decl.name == match->name) {
                constraints.push_back(*match);
                break;
            }
        }
    }
}

// A parameter override for a default-instantiated interface port, with the value/type already
// resolved in the instantiating scope. Applied directly to the parameter builder.
struct IfaceParamOverride {
    std::string_view name;
    std::variant<ConstantValue, const Type*> value;
};

// Resolves each constraint's operand in the instantiating scope (where it was written) into a
// concrete value or type.
static void resolveIfaceParamOverrides(std::span<const IfaceParamConstraint> constraints,
                                       const ASTContext& context,
                                       SmallVectorBase<IfaceParamOverride>& overrides) {
    for (auto& c : constraints) {
        if (c.isType) {
            auto& dataType = c.operand->as<syntax::DataTypeSyntax>();
            auto& type = context.getCompilation().getType(dataType, context);
            if (!type.isError())
                overrides.push_back({c.name, &type});
        }
        else {
            auto& expr = Expression::bind(*c.operand, context);
            if (auto value = context.tryEval(expr))
                overrides.push_back({c.name, std::move(value)});
        }
    }
}

static Symbol* recurseDefaultIfaceInst(Compilation& comp, const InterfacePortSymbol& port,
                                       std::span<const IfaceParamOverride> paramOverrides,
                                       const InstanceSymbol*& firstInst,
                                       std::span<const ConstantRange>::iterator it,
                                       std::span<const ConstantRange>::iterator end) {
    if (it == end) {
        auto& def = *port.interfaceDef;
        ParameterBuilder paramBuilder(*def.getParentScope(), def.name, def.parameters);
        paramBuilder.setUseInvalidForMissing(true);

        // Apply any `$static_assert`-derived overrides. The values were already resolved in the
        // instantiating scope, so they're applied directly without an instance context.
        for (auto& ov : paramOverrides) {
            if (auto type = std::get_if<const Type*>(&ov.value))
                paramBuilder.addTypeOverride(ov.name, **type);
            else
                paramBuilder.addValueOverride(ov.name, std::get<ConstantValue>(ov.value));
        }

        auto& body = InstanceBodySymbol::fromDefinition(comp, def, port.location, paramBuilder,
                                                        InstanceFlags::None);

        auto& result = *comp.emplace<InstanceSymbol>(port.name, port.location, body, 0u);

        if (!firstInst)
            firstInst = &result;
        return &result;
    }

    ConstantRange range = *it++;
    if (range.width() > comp.getOptions().maxInstanceArray)
        return &InstanceArraySymbol::createEmpty(comp, port.name, port.location);

    SmallVector<const Symbol*> elements;
    for (uint32_t i = 0; i < range.width(); i++) {
        auto symbol = recurseDefaultIfaceInst(comp, port, paramOverrides, firstInst, it, end);
        symbol->name = "";
        elements.push_back(symbol);
    }

    auto result = comp.emplace<InstanceArraySymbol>(comp, port.name, port.location,
                                                    elements.copy(comp), range);
    for (auto element : elements)
        result->addMember(*element);

    return result;
}

void InstanceSymbol::connectDefaultIfacePorts() const {
    auto parent = getParentScope();
    SLANG_ASSERT(parent);

    auto& comp = parent->getCompilation();
    ASTContext context(body, LookupLocation::max);

    // The body of the instantiating module may contain `$static_assert` constraints that
    // pin interface-port parameters to specific values; we honor those when synthesizing
    // the default interface instances below. Ideally one day the LRM will allow
    // specifying these constraints in the port declaration itself.
    auto bodySyntax = body.getSyntax() ? body.getSyntax()->as_if<syntax::ModuleDeclarationSyntax>()
                                       : nullptr;

    SmallVector<const PortConnection*> conns;
    for (auto port : body.getPortList()) {
        if (port->kind == SymbolKind::InterfacePort) {
            auto& ifacePort = port->as<InterfacePortSymbol>();
            if (ifacePort.interfaceDef) {
                SmallVector<IfaceParamConstraint> constraints;
                SmallVector<IfaceParamOverride> paramOverrides;
                if (bodySyntax) {
                    collectIfaceParamConstraints(ifacePort, *bodySyntax, constraints);
                    resolveIfaceParamOverrides(constraints, context, paramOverrides);
                }

                Symbol* inst;
                const ModportSymbol* modport = nullptr;
                if (auto dims = ifacePort.getDeclaredRange()) {
                    const InstanceSymbol* firstInst = nullptr;
                    inst = recurseDefaultIfaceInst(comp, ifacePort, paramOverrides, firstInst,
                                                   dims->begin(), dims->end());

                    if (firstInst) {
                        auto portRange = SourceRange{port->location,
                                                     port->location + port->name.length()};
                        modport = ifacePort.getModport(context, *firstInst, portRange);
                    }
                }
                else {
                    inst = &InstanceArraySymbol::createEmpty(comp, port->name, port->location);
                }

                inst->setParent(*parent);
                conns.emplace_back(
                    comp.emplace<PortConnection>(ifacePort, std::pair{inst, modport}, nullptr));
                connectionMap->emplace(reinterpret_cast<uintptr_t>(port),
                                       reinterpret_cast<uintptr_t>(conns.back()));
            }
        }
    }
    connections = conns.copy(comp);
}

} // namespace slang::ast
