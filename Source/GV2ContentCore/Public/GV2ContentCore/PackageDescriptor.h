#pragma once

#include "GV2ContentCore/GV2ContentCore.h"
#include "GV2ContentCore/Diagnostic.h"

#include <cstdint>
#include <string>
#include <vector>

namespace GV2ContentCore
{
    struct GV2_CONTENT_CORE_API FRedirectDescriptor final
    {
        FRedirectDescriptor(std::string InSourceId, std::string InTargetId)
            : SourceId(std::move(InSourceId)), TargetId(std::move(InTargetId)) {}

        const std::string& GetSourceId() const { return SourceId; }
        const std::string& GetTargetId() const { return TargetId; }
        bool operator==(const FRedirectDescriptor&) const = default;

    private:
        std::string SourceId;
        std::string TargetId;
    };

    /**
     * PKG-03: one entry from a manifest's "dependencies" list — parsed and
     * form-validated only; whether the named package is actually present
     * and cycle-free is checked against a package set (plan
     * PackageSupport, M2 Discovery and Order).
     */
    struct GV2_CONTENT_CORE_API FPackageDependency final
    {
        FPackageDependency(std::string InPackageId, bool bInLoadAfter)
            : PackageId(std::move(InPackageId)), bLoadAfter(bInLoadAfter) {}

        const std::string& GetPackageId() const { return PackageId; }
        // load_after is an editor ordering hint only (Modding.md "Load
        // order"); runtime load order is never changed silently by it.
        bool GetLoadAfter() const { return bLoadAfter; }
        bool operator==(const FPackageDependency&) const = default;

    private:
        std::string PackageId;
        bool bLoadAfter = false;
    };

    /**
     * Exact schema binding for a content package.
     */
    struct GV2_CONTENT_CORE_API FSchemaBinding final
    {
        FSchemaBinding(
            std::string InDefinitionType,
            std::int64_t InSchemaVersion,
            std::string InSchemaId,
            std::string InRelativePath);
        FSchemaBinding(const FSchemaBinding&) = default;
        FSchemaBinding(FSchemaBinding&&) noexcept = default;
        FSchemaBinding& operator=(const FSchemaBinding&) = delete;
        FSchemaBinding& operator=(FSchemaBinding&&) = delete;

        const std::string& GetDefinitionType() const { return DefinitionType; }
        std::int64_t GetSchemaVersion() const { return SchemaVersion; }
        const std::string& GetSchemaId() const { return SchemaId; }
        const std::string& GetRelativePath() const { return RelativePath; }

        bool operator==(const FSchemaBinding& Other) const;
        bool operator!=(const FSchemaBinding& Other) const;

    private:
        std::string DefinitionType;
        std::int64_t SchemaVersion = 0;
        std::string SchemaId;
        std::string RelativePath;
    };

    /** Exact binding for one package-owned extension block at one schema site. */
    struct GV2_CONTENT_CORE_API FExtensionSchemaBinding final
    {
        FExtensionSchemaBinding(
            std::string InDefinitionType,
            std::int64_t InSchemaVersion,
            std::string InExtensionSite,
            std::string InExtensionNamespace,
            std::string InSchemaId,
            std::string InRelativePath);
        FExtensionSchemaBinding(const FExtensionSchemaBinding&) = default;
        FExtensionSchemaBinding(FExtensionSchemaBinding&&) noexcept = default;
        FExtensionSchemaBinding& operator=(const FExtensionSchemaBinding&) = delete;
        FExtensionSchemaBinding& operator=(FExtensionSchemaBinding&&) = delete;

        const std::string& GetDefinitionType() const { return DefinitionType; }
        std::int64_t GetSchemaVersion() const { return SchemaVersion; }
        const std::string& GetExtensionSite() const { return ExtensionSite; }
        const std::string& GetExtensionNamespace() const { return ExtensionNamespace; }
        const std::string& GetSchemaId() const { return SchemaId; }
        const std::string& GetRelativePath() const { return RelativePath; }

        bool operator==(const FExtensionSchemaBinding& Other) const;
        bool operator!=(const FExtensionSchemaBinding& Other) const;

    private:
        std::string DefinitionType;
        std::int64_t SchemaVersion = 0;
        std::string ExtensionSite;
        std::string ExtensionNamespace;
        std::string SchemaId;
        std::string RelativePath;
    };

    /**
     * Immutable resolved package descriptor.
     * Contains package metadata, load order index, relative source paths and schema bindings.
     * Note: Descriptor does NOT perform filesystem discovery or compute mod loading order.
     */
    struct GV2_CONTENT_CORE_API FPackageDescriptor final
    {
        FPackageDescriptor(
            std::string InPackageId,
            std::string InNamespace,
            std::uint32_t InLoadIndex,
            std::vector<std::string> InRelativeSources = {},
            std::vector<FSchemaBinding> InSchemaBindings = {},
            std::vector<FExtensionSchemaBinding> InExtensionSchemaBindings = {},
            std::vector<FRedirectDescriptor> InRedirects = {},
            std::vector<std::string> InTombstones = {},
            // PKG-01/03: manifest-owned identity/dependency fields. Default
            // to empty so every existing programmatic call site (tests,
            // conformance fixtures that never go through manifest
            // discovery) keeps compiling unchanged.
            std::string InVersion = {},
            std::vector<FPackageDependency> InDependencies = {});
        FPackageDescriptor(const FPackageDescriptor&) = default;
        FPackageDescriptor(FPackageDescriptor&&) noexcept = default;
        FPackageDescriptor& operator=(const FPackageDescriptor&) = delete;
        FPackageDescriptor& operator=(FPackageDescriptor&&) = delete;

        const std::string& GetPackageId() const { return PackageId; }
        const std::string& GetNamespace() const { return Namespace; }
        std::uint32_t GetLoadIndex() const { return LoadIndex; }
        const std::vector<std::string>& GetRelativeSources() const { return RelativeSources; }
        const std::vector<FSchemaBinding>& GetSchemaBindings() const { return SchemaBindings; }
        const std::vector<FExtensionSchemaBinding>& GetExtensionSchemaBindings() const { return ExtensionSchemaBindings; }
        const std::vector<FRedirectDescriptor>& GetRedirects() const { return Redirects; }
        const std::vector<std::string>& GetTombstones() const { return Tombstones; }
        const std::string& GetVersion() const { return Version; }
        const std::vector<FPackageDependency>& GetDependencies() const { return Dependencies; }

        FPackageDescriptor WithLoadIndex(std::uint32_t NewLoadIndex) const;

        bool operator==(const FPackageDescriptor& Other) const;
        bool operator!=(const FPackageDescriptor& Other) const;

    private:
        std::string PackageId;
        std::string Namespace;
        std::uint32_t LoadIndex = 0;
        std::vector<std::string> RelativeSources;
        std::vector<FSchemaBinding> SchemaBindings;
        std::vector<FExtensionSchemaBinding> ExtensionSchemaBindings;
        std::vector<FRedirectDescriptor> Redirects;
        std::vector<std::string> Tombstones;
        std::string Version;
        std::vector<FPackageDependency> Dependencies;
    };

    /**
     * Validates a set of resolved package descriptors according to PCC package rules.
     * Returns empty diagnostic vector if valid, or ordered typed diagnostics if invalid.
     */
    GV2_CONTENT_CORE_API std::vector<FDiagnostic> ValidatePackageDescriptors(
        const std::vector<FPackageDescriptor>& Descriptors);
}
