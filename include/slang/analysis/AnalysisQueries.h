//------------------------------------------------------------------------------
//! @file AnalysisQueries.h
//! @brief Instance-aware queries over completed analysis results
//
// SPDX-FileCopyrightText: Michael Popoloski
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------------
#pragma once

#include <optional>
#include <vector>

#include "slang/util/BumpAllocator.h"
#include "slang/util/FlatMap.h"

namespace slang::ast {

class Compilation;
class InstanceBodySymbol;
class Scope;
class Symbol;
class ValueSymbol;

} // namespace slang::ast

namespace slang::analysis {

class AnalysisManager;
class ValueDriver;

/// Queries completed analysis results, localizing drivers for non-canonical instances.
/// Queries can lazily elaborate the compilation and must be serialized with all other
/// access to it, including queries through other AnalysisQueries objects.
class SLANG_EXPORT AnalysisQueries {
public:
    /// Constructs queries for a compilation whose analysis has completed and which is unfrozen.
    /// Both arguments must outlive this object. Recreate it if the design or analysis changes.
    AnalysisQueries(ast::Compilation& compilation, const AnalysisManager& analysis);

    /// Returns drivers for the symbol, including drivers from its canonical instance body.
    /// The symbol must belong to this compilation, which must remain unfrozen during the call.
    /// Returned drivers are valid for the lifetime of this object and its analysis manager.
    std::vector<const ValueDriver*> getDrivers(const ast::ValueSymbol& symbol);

    /// Returns the estimated memory used by query caches and localized driver copies.
    size_t getMemoryUsage() const;

private:
    /// Canonical and enclosing relationships for one instance body.
    struct CanonicalBodyInfo {
        /// The canonical counterpart for this instance body.
        const ast::InstanceBodySymbol* canonicalBody;

        /// The enclosing non-canonical body whose mapping continues this body's hierarchy.
        const ast::InstanceBodySymbol* parentBody;
    };

    /// Bidirectional symbol mappings for one non-canonical instance body.
    struct CanonicalBodyMapping {
        /// Maps symbols from the canonical body to the local body.
        flat_hash_map<const ast::Symbol*, const ast::Symbol*> canonicalToLocal;

        /// Maps symbols from the local body to the canonical body.
        flat_hash_map<const ast::Symbol*, const ast::Symbol*> localToCanonical;
    };

    /// Ensures that the given body has a populated canonical symbol mapping.
    void ensureBodyMapping(const ast::InstanceBodySymbol& body);

    /// Records corresponding symbols in paired local and canonical scopes.
    void populatePairedScopes(const ast::Scope& local, const ast::Scope& canonical,
                              const ast::InstanceBodySymbol& mappingBody);

    /// Finds and caches the canonical relationship for the given body.
    CanonicalBodyInfo getCanonicalBodyInfo(const ast::InstanceBodySymbol& body);

    /// Finds the symbol corresponding to a canonical symbol in a local hierarchy.
    const ast::Symbol* getLocalSymbol(const ast::Symbol& canonical,
                                      const ast::InstanceBodySymbol& localBody);

    /// The unfrozen compilation used for lazy instance elaboration.
    ast::Compilation& compilation;

    /// Completed analysis results used to look up recorded drivers.
    const AnalysisManager& analysis;

    /// Owns localized driver copies created for non-canonical symbols.
    std::optional<BumpAllocator> alloc;

    /// Stores canonical and parent relationships for instance bodies.
    flat_hash_map<const ast::InstanceBodySymbol*, CanonicalBodyInfo> bodyInfo;

    /// Stores paired symbol mappings for instance bodies.
    flat_hash_map<const ast::InstanceBodySymbol*, CanonicalBodyMapping> bodyMappings;

    /// Stores completed localized driver queries by target symbol.
    flat_hash_map<const ast::ValueSymbol*, std::vector<const ValueDriver*>> drivers;
};

} // namespace slang::analysis
