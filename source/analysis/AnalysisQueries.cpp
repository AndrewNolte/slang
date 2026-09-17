//------------------------------------------------------------------------------
// AnalysisQueries.cpp
// Instance-aware queries over completed analysis results
//
// SPDX-FileCopyrightText: Michael Popoloski
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------
#include "slang/analysis/AnalysisQueries.h"

#include "slang/analysis/AnalysisManager.h"
#include "slang/analysis/ValueDriver.h"
#include "slang/ast/Compilation.h"
#include "slang/ast/EvalContext.h"
#include "slang/ast/symbols/InstanceSymbols.h"
#include "slang/ast/symbols/ValueSymbol.h"

namespace slang::analysis {

using namespace ast;

AnalysisQueries::AnalysisQueries(Compilation& compilation, const AnalysisManager& analysis) :
    compilation(compilation), analysis(analysis) {
    SLANG_ASSERT(compilation.isElaborated());
    SLANG_ASSERT(!compilation.isFrozen());
}

std::vector<const ValueDriver*> AnalysisQueries::getDrivers(const ValueSymbol& symbol) {
    SLANG_ASSERT(!compilation.isFrozen());
    auto directDrivers = analysis.getDrivers(symbol);
    const Scope* scope = symbol.getParentScope();
    if (!scope)
        return directDrivers;

    SLANG_ASSERT(&scope->getCompilation() == &compilation);
    const InstanceBodySymbol* localBody = scope->getContainingInstance();
    if (!localBody)
        return directDrivers;

    if (auto it = drivers.find(&symbol); it != drivers.end())
        return it->second;

    auto info = getCanonicalBodyInfo(*localBody);
    if (info.canonicalBody == localBody && !info.parentBody)
        return directDrivers;

    ensureBodyMapping(*localBody);
    auto& mapping = bodyMappings.at(localBody);
    auto canonicalIt = mapping.localToCanonical.find(&symbol);
    if (canonicalIt == mapping.localToCanonical.end() || canonicalIt->second == &symbol)
        return directDrivers;

    auto& canonicalSymbol = canonicalIt->second->as<ValueSymbol>();
    for (const ValueDriver* canonicalDriver : analysis.getDrivers(canonicalSymbol)) {
        const Symbol* localContaining = getLocalSymbol(*canonicalDriver->containingSymbol,
                                                       *localBody);
        // Drivers originating outside the canonical subtree can apply to only one occurrence.
        if (!localContaining)
            continue;

        EvalContext evalContext(*localContaining);
        if (!alloc)
            alloc.emplace();
        auto* localDriver = ValueDriver::create(*alloc, evalContext, *canonicalDriver, symbol);
        localDriver->containingSymbol = localContaining;
        directDrivers.push_back(localDriver);
    }

    auto [it, _] = drivers.emplace(&symbol, std::move(directDrivers));
    return it->second;
}

AnalysisQueries::CanonicalBodyInfo AnalysisQueries::getCanonicalBodyInfo(
    const InstanceBodySymbol& body) {

    if (auto it = bodyInfo.find(&body); it != bodyInfo.end())
        return it->second;

    SmallVector<const InstanceBodySymbol*, 4> path;
    const InstanceBodySymbol* current = &body;
    size_t mappingRootIndex = size_t(-1);
    const InstanceBodySymbol* canonicalRoot = nullptr;
    while (current) {
        path.push_back(current);
        if (auto it = bodyInfo.find(current); it != bodyInfo.end()) {
            mappingRootIndex = path.size() - 1;
            canonicalRoot = nullptr;
            break;
        }

        if (current->parentInstance) {
            if (const InstanceBodySymbol* canonical = current->parentInstance->getCanonicalBody()) {
                mappingRootIndex = path.size() - 1;
                canonicalRoot = canonical;
            }
        }

        const Scope* parentScope = current->parentInstance
                                       ? current->parentInstance->getParentScope()
                                       : nullptr;
        if (!parentScope)
            break;
        current = parentScope->getContainingInstance();
    }

    if (mappingRootIndex != size_t(-1)) {
        const InstanceBodySymbol* mappingRoot = path[mappingRootIndex];
        if (canonicalRoot) {
            bodyInfo.emplace(mappingRoot, CanonicalBodyInfo{canonicalRoot, nullptr});
        }

        for (size_t i = mappingRootIndex + 1; i > 0; i--)
            ensureBodyMapping(*path[i - 1]);
    }

    if (auto it = bodyInfo.find(&body); it != bodyInfo.end())
        return it->second;

    auto info = CanonicalBodyInfo{&body, nullptr};
    bodyInfo.emplace(&body, info);
    return info;
}

void AnalysisQueries::ensureBodyMapping(const InstanceBodySymbol& body) {
    if (bodyMappings.contains(&body))
        return;

    auto infoIt = bodyInfo.find(&body);
    SLANG_ASSERT(infoIt != bodyInfo.end());
    bodyMappings.emplace(&body, CanonicalBodyMapping{});
    populatePairedScopes(body, *infoIt->second.canonicalBody, body);
}

const Symbol* AnalysisQueries::getLocalSymbol(const Symbol& canonical,
                                              const InstanceBodySymbol& localBody) {
    const InstanceBodySymbol* current = &localBody;
    while (current) {
        ensureBodyMapping(*current);
        auto& mapping = bodyMappings.at(current);
        if (auto it = mapping.canonicalToLocal.find(&canonical);
            it != mapping.canonicalToLocal.end()) {
            return it->second;
        }

        current = bodyInfo.at(current).parentBody;
    }

    const Scope* canonicalScope = canonical.getParentScope();
    if (!canonicalScope)
        return nullptr;

    const InstanceBodySymbol* canonicalBody = canonicalScope->getContainingInstance();
    if (!canonicalBody || !canonicalBody->parentInstance)
        return nullptr;

    auto* localInstance = getLocalSymbol(*canonicalBody->parentInstance, localBody);
    if (!localInstance || localInstance->kind != SymbolKind::Instance)
        return nullptr;

    auto& descendantBody = localInstance->as<InstanceSymbol>().body;
    getCanonicalBodyInfo(descendantBody);
    ensureBodyMapping(descendantBody);
    auto& descendantMapping = bodyMappings.at(&descendantBody);
    if (auto it = descendantMapping.canonicalToLocal.find(&canonical);
        it != descendantMapping.canonicalToLocal.end()) {
        return it->second;
    }
    return nullptr;
}

void AnalysisQueries::populatePairedScopes(const Scope& local, const Scope& canonical,
                                           const InstanceBodySymbol& mappingBody) {
    auto& mapping = bodyMappings.at(&mappingBody);
    mapping.canonicalToLocal.emplace(&canonical.asSymbol(), &local.asSymbol());
    mapping.localToCanonical.emplace(&local.asSymbol(), &canonical.asSymbol());

    auto localIt = local.members().begin();
    auto localEnd = local.members().end();
    auto canonIt = canonical.members().begin();
    auto canonEnd = canonical.members().end();
    for (; localIt != localEnd && canonIt != canonEnd; ++localIt, ++canonIt) {
        if (localIt->kind != canonIt->kind)
            return;

        mapping.canonicalToLocal.emplace(&*canonIt, &*localIt);
        mapping.localToCanonical.emplace(&*localIt, &*canonIt);

        if (localIt->kind == SymbolKind::Instance) {
            auto& localInstance = localIt->as<InstanceSymbol>();
            auto& canonicalInstance = canonIt->as<InstanceSymbol>();
            const InstanceBodySymbol* canonicalBody = canonicalInstance.getCanonicalBody();
            if (!canonicalBody)
                canonicalBody = &canonicalInstance.body;

            const auto& mappingInfo = bodyInfo.at(&mappingBody);
            const InstanceBodySymbol* parentBody = mappingInfo.canonicalBody == &mappingBody
                                                       ? nullptr
                                                       : &mappingBody;
            bodyInfo.emplace(&localInstance.body, CanonicalBodyInfo{canonicalBody, parentBody});
            continue;
        }

        if (localIt->isScope())
            populatePairedScopes(localIt->as<Scope>(), canonIt->as<Scope>(), mappingBody);
    }
}

size_t AnalysisQueries::getMemoryUsage() const {
    auto mapBytes = [](const auto& map) {
        return map.size() * sizeof(typename std::remove_reference_t<decltype(map)>::value_type);
    };
    size_t result = alloc ? alloc->getTotalBytesAllocated() : 0;
    result += mapBytes(bodyInfo);
    result += mapBytes(bodyMappings);
    result += mapBytes(drivers);
    for (auto& [_, mapping] : bodyMappings) {
        result += mapBytes(mapping.canonicalToLocal);
        result += mapBytes(mapping.localToCanonical);
    }
    for (auto& [_, values] : drivers)
        result += values.capacity() * sizeof(const ValueDriver*);
    return result;
}

} // namespace slang::analysis
