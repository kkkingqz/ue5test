#include "GV2ContentCore/PackageDescriptor.h"

#include "GV2ContentCore/StableId.h"

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <tuple>

namespace GV2ContentCore
{
namespace
{
constexpr const char* DuplicatePackageIdCode = "core:diagnostic.repository.package_set.duplicate_id";
constexpr const char* DuplicateLoadIndexCode = "core:diagnostic.repository.package_set.duplicate_load_index";
constexpr const char* InvalidPackageIdCode = "core:diagnostic.repository.package_set.invalid_package_id";
constexpr const char* InvalidNamespaceCode = "core:diagnostic.repository.package_set.invalid_namespace";
constexpr const char* NamespaceMismatchCode = "core:diagnostic.repository.package_set.namespace_mismatch";
constexpr const char* InvalidCoreIndexCode = "core:diagnostic.repository.package_set.invalid_core_index";
constexpr const char* MissingCoreCode = "core:diagnostic.repository.package_set.missing_core";
constexpr const char* InvalidRelativePathCode = "core:diagnostic.repository.package_set.invalid_relative_path";
constexpr const char* InvalidSchemaBindingCode = "core:diagnostic.repository.package_set.invalid_schema_binding";
constexpr const char* DuplicateSchemaBindingCode = "core:diagnostic.repository.package_set.duplicate_schema_binding";
constexpr const char* InvalidExtensionSchemaBindingCode = "core:diagnostic.repository.package_set.invalid_extension_schema_binding";
constexpr const char* DuplicateExtensionSchemaBindingCode = "core:diagnostic.repository.package_set.duplicate_extension_schema_binding";
constexpr const char* InvalidRedirectCode = "core:diagnostic.repository.redirect.invalid";
constexpr const char* ForeignRedirectCode = "core:diagnostic.repository.redirect.foreign_source";
constexpr const char* RedirectKindMismatchCode = "core:diagnostic.repository.redirect.kind_mismatch";
constexpr const char* RedirectConflictCode = "core:diagnostic.repository.redirect.conflict";
constexpr const char* InvalidTombstoneCode = "core:diagnostic.repository.tombstone.invalid";
constexpr const char* ForeignTombstoneCode = "core:diagnostic.repository.tombstone.foreign_id";
constexpr const char* MissingDependencyCode = "core:diagnostic.package.order.missing_dependency";
constexpr const char* DependencyCycleCode = "core:diagnostic.package.order.dependency_cycle";
constexpr const char* LoadAfterViolationCode = "core:diagnostic.package.order.load_after_violation";

bool IsCanonicalRelativePath(const std::string& Path)
{
    if (Path.empty() || Path.front() == '/' || Path.back() == '/' || Path.find('\\') != std::string::npos)
    {
        return false;
    }

    std::size_t Start = 0;
    while (Start <= Path.size())
    {
        const std::size_t Slash = Path.find('/', Start);
        const std::size_t End = Slash == std::string::npos ? Path.size() : Slash;
        const std::string_view Segment(Path.data() + Start, End - Start);
        if (Segment.empty() || Segment == "." || Segment == "..")
        {
            return false;
        }
        if (Slash == std::string::npos)
        {
            break;
        }
        Start = Slash + 1;
    }
    return true;
}

FDiagnostic MakePackageDiagnostic(
    const FPackageDescriptor& Descriptor,
    const char* Code,
    std::string Message,
    std::optional<std::string> RelativeSource = std::nullopt)
{
    FDiagnostic Diagnostic;
    Diagnostic.Code = Code;
    Diagnostic.Message = std::move(Message);
    Diagnostic.PackageId = Descriptor.GetPackageId();
    Diagnostic.PackageLoadIndex = Descriptor.GetLoadIndex();
    Diagnostic.RelativeSource = std::move(RelativeSource);
    return Diagnostic;
}
}

FSchemaBinding::FSchemaBinding(
    std::string InDefinitionType,
    const std::int64_t InSchemaVersion,
    std::string InSchemaId,
    std::string InRelativePath)
    : DefinitionType(std::move(InDefinitionType))
    , SchemaVersion(InSchemaVersion)
    , SchemaId(std::move(InSchemaId))
    , RelativePath(std::move(InRelativePath))
{
}

bool FSchemaBinding::operator==(const FSchemaBinding& Other) const
{
    return DefinitionType == Other.DefinitionType
        && SchemaVersion == Other.SchemaVersion
        && SchemaId == Other.SchemaId
        && RelativePath == Other.RelativePath;
}

bool FSchemaBinding::operator!=(const FSchemaBinding& Other) const
{
    return !(*this == Other);
}

FExtensionSchemaBinding::FExtensionSchemaBinding(
    std::string InDefinitionType,
    const std::int64_t InSchemaVersion,
    std::string InExtensionSite,
    std::string InExtensionNamespace,
    std::string InSchemaId,
    std::string InRelativePath)
    : DefinitionType(std::move(InDefinitionType))
    , SchemaVersion(InSchemaVersion)
    , ExtensionSite(std::move(InExtensionSite))
    , ExtensionNamespace(std::move(InExtensionNamespace))
    , SchemaId(std::move(InSchemaId))
    , RelativePath(std::move(InRelativePath))
{
}

bool FExtensionSchemaBinding::operator==(const FExtensionSchemaBinding& Other) const
{
    return DefinitionType == Other.DefinitionType
        && SchemaVersion == Other.SchemaVersion
        && ExtensionSite == Other.ExtensionSite
        && ExtensionNamespace == Other.ExtensionNamespace
        && SchemaId == Other.SchemaId
        && RelativePath == Other.RelativePath;
}

bool FExtensionSchemaBinding::operator!=(const FExtensionSchemaBinding& Other) const
{
    return !(*this == Other);
}

FPackageDescriptor::FPackageDescriptor(
    std::string InPackageId,
    std::string InNamespace,
    const std::uint32_t InLoadIndex,
    std::vector<std::string> InRelativeSources,
    std::vector<FSchemaBinding> InSchemaBindings,
    std::vector<FExtensionSchemaBinding> InExtensionSchemaBindings,
    std::vector<FRedirectDescriptor> InRedirects,
    std::vector<std::string> InTombstones,
    std::string InVersion,
    std::vector<FPackageDependency> InDependencies)
    : PackageId(std::move(InPackageId))
    , Namespace(std::move(InNamespace))
    , LoadIndex(InLoadIndex)
    , RelativeSources(std::move(InRelativeSources))
    , SchemaBindings(std::move(InSchemaBindings))
    , ExtensionSchemaBindings(std::move(InExtensionSchemaBindings))
    , Redirects(std::move(InRedirects))
    , Tombstones(std::move(InTombstones))
    , Version(std::move(InVersion))
    , Dependencies(std::move(InDependencies))
{
}

bool FPackageDescriptor::operator==(const FPackageDescriptor& Other) const
{
    return PackageId == Other.PackageId
        && Namespace == Other.Namespace
        && LoadIndex == Other.LoadIndex
        && RelativeSources == Other.RelativeSources
        && SchemaBindings == Other.SchemaBindings
        && ExtensionSchemaBindings == Other.ExtensionSchemaBindings
        && Redirects == Other.Redirects
        && Tombstones == Other.Tombstones
        && Version == Other.Version
        && Dependencies == Other.Dependencies;
}

bool FPackageDescriptor::operator!=(const FPackageDescriptor& Other) const
{
    return !(*this == Other);
}

FPackageDescriptor FPackageDescriptor::WithLoadIndex(const std::uint32_t NewLoadIndex) const
{
    return FPackageDescriptor(
        PackageId,
        Namespace,
        NewLoadIndex,
        RelativeSources,
        SchemaBindings,
        ExtensionSchemaBindings,
        Redirects,
        Tombstones,
        Version,
        Dependencies);
}

std::vector<FDiagnostic> ValidatePackageDescriptors(
    const std::vector<FPackageDescriptor>& Descriptors)
{
    std::vector<FDiagnostic> Diagnostics;
    std::map<std::string, std::size_t> PackageIdCounts;
    std::map<std::uint32_t, std::size_t> LoadIndexCounts;
    for (const FPackageDescriptor& Descriptor : Descriptors)
    {
        ++PackageIdCounts[Descriptor.GetPackageId()];
        ++LoadIndexCounts[Descriptor.GetLoadIndex()];
    }

    std::vector<const FPackageDescriptor*> OrderedDescriptors;
    OrderedDescriptors.reserve(Descriptors.size());
    for (const FPackageDescriptor& Descriptor : Descriptors)
    {
        OrderedDescriptors.push_back(&Descriptor);
    }
    std::sort(
        OrderedDescriptors.begin(),
        OrderedDescriptors.end(),
        [](const FPackageDescriptor* Left, const FPackageDescriptor* Right)
        {
            return std::tuple(Left->GetLoadIndex(), Left->GetPackageId(), Left->GetNamespace())
                < std::tuple(Right->GetLoadIndex(), Right->GetPackageId(), Right->GetNamespace());
        });

    bool bHasCoreAtZero = false;
    for (const FPackageDescriptor* DescriptorPointer : OrderedDescriptors)
    {
        const FPackageDescriptor& Descriptor = *DescriptorPointer;
        const std::string& PackageId = Descriptor.GetPackageId();
        const std::string& Namespace = Descriptor.GetNamespace();

        if (!FStableId::IsValidSegment(PackageId))
        {
            Diagnostics.push_back(MakePackageDiagnostic(
                Descriptor,
                InvalidPackageIdCode,
                "package_id is not a canonical Stable ID segment"));
        }
        if (!FStableId::IsValidSegment(Namespace))
        {
            Diagnostics.push_back(MakePackageDiagnostic(
                Descriptor,
                InvalidNamespaceCode,
                "namespace is not a canonical Stable ID segment"));
        }
        else if (Namespace != PackageId)
        {
            Diagnostics.push_back(MakePackageDiagnostic(
                Descriptor,
                NamespaceMismatchCode,
                "package namespace must equal package_id"));
        }
        if (PackageIdCounts[PackageId] > 1)
        {
            Diagnostics.push_back(MakePackageDiagnostic(
                Descriptor,
                DuplicatePackageIdCode,
                "Duplicate package_id: " + PackageId));
        }
        if (LoadIndexCounts[Descriptor.GetLoadIndex()] > 1)
        {
            Diagnostics.push_back(MakePackageDiagnostic(
                Descriptor,
                DuplicateLoadIndexCode,
                "Duplicate load_index: " + std::to_string(Descriptor.GetLoadIndex())));
        }

        if (PackageId == "core")
        {
            if (Descriptor.GetLoadIndex() == 0 && Namespace == "core")
            {
                bHasCoreAtZero = true;
            }
            else if (Descriptor.GetLoadIndex() != 0)
            {
                Diagnostics.push_back(MakePackageDiagnostic(
                    Descriptor,
                    InvalidCoreIndexCode,
                    "Core package must have load_index 0"));
            }
        }

        std::set<std::string> SeenSources;
        for (const std::string& RelativeSource : Descriptor.GetRelativeSources())
        {
            if (!IsCanonicalRelativePath(RelativeSource) || !SeenSources.insert(RelativeSource).second)
            {
                Diagnostics.push_back(MakePackageDiagnostic(
                    Descriptor,
                    InvalidRelativePathCode,
                    "Source path must be unique and canonical package-relative",
                    RelativeSource));
            }
        }

        std::set<std::pair<std::string, std::int64_t>> SeenBindingKeys;
        for (const FSchemaBinding& Binding : Descriptor.GetSchemaBindings())
        {
            const bool bValid = FStableId::IsValidSegment(Binding.GetDefinitionType())
                && Binding.GetSchemaVersion() > 0
                && FStableId::IsOfKind(Binding.GetSchemaId(), "schema")
                && IsCanonicalRelativePath(Binding.GetRelativePath());
            if (!bValid)
            {
                FDiagnostic Diagnostic = MakePackageDiagnostic(
                    Descriptor,
                    InvalidSchemaBindingCode,
                    "Schema binding must contain a canonical definition_type, positive schema_version, schema_id and package-relative path",
                    Binding.GetRelativePath());
                Diagnostic.SchemaId = Binding.GetSchemaId();
                Diagnostic.SchemaVersion = Binding.GetSchemaVersion();
                Diagnostics.push_back(std::move(Diagnostic));
            }
            if (!SeenSources.insert(Binding.GetRelativePath()).second)
            {
                Diagnostics.push_back(MakePackageDiagnostic(
                    Descriptor,
                    InvalidRelativePathCode,
                    "Definition and schema source paths must be unique within a package",
                    Binding.GetRelativePath()));
            }
            if (!SeenBindingKeys.emplace(Binding.GetDefinitionType(), Binding.GetSchemaVersion()).second)
            {
                FDiagnostic Diagnostic = MakePackageDiagnostic(
                    Descriptor,
                    DuplicateSchemaBindingCode,
                    "Package contains more than one schema binding for (definition_type, schema_version): "
                        + Binding.GetDefinitionType() + ", " + std::to_string(Binding.GetSchemaVersion()),
                    Binding.GetRelativePath());
                Diagnostic.SchemaId = Binding.GetSchemaId();
                Diagnostic.SchemaVersion = Binding.GetSchemaVersion();
                Diagnostics.push_back(std::move(Diagnostic));
            }
        }

        std::set<std::tuple<std::string, std::int64_t, std::string, std::string>> SeenExtensionBindingKeys;
        for (const FExtensionSchemaBinding& Binding : Descriptor.GetExtensionSchemaBindings())
        {
            FStableIdView ParsedSchemaId;
            const bool bSchemaIdValid = FStableId::Parse(Binding.GetSchemaId(), ParsedSchemaId)
                && ParsedSchemaId.Kind == "schema"
                && ParsedSchemaId.Namespace == Namespace;
            const bool bSiteValid = Binding.GetExtensionSite() == "definition_file"
                || Binding.GetExtensionSite() == "definition_entry"
                || Binding.GetExtensionSite() == "schema_resource";
            const bool bValid = FStableId::IsValidSegment(Binding.GetDefinitionType())
                && Binding.GetSchemaVersion() > 0
                && bSiteValid
                && Binding.GetExtensionNamespace() == Namespace
                && bSchemaIdValid
                && IsCanonicalRelativePath(Binding.GetRelativePath());
            if (!bValid)
            {
                FDiagnostic Diagnostic = MakePackageDiagnostic(
                    Descriptor,
                    InvalidExtensionSchemaBindingCode,
                    "Extension schema binding must be canonical, package-owned and use a supported extension_site",
                    Binding.GetRelativePath());
                Diagnostic.SchemaId = Binding.GetSchemaId();
                Diagnostic.SchemaVersion = Binding.GetSchemaVersion();
                Diagnostics.push_back(std::move(Diagnostic));
            }
            if (!SeenSources.insert(Binding.GetRelativePath()).second)
            {
                Diagnostics.push_back(MakePackageDiagnostic(
                    Descriptor,
                    InvalidRelativePathCode,
                    "Definition, schema and extension schema source paths must be unique within a package",
                    Binding.GetRelativePath()));
            }
            const auto Key = std::tuple(
                Binding.GetDefinitionType(),
                Binding.GetSchemaVersion(),
                Binding.GetExtensionSite(),
                Binding.GetExtensionNamespace());
            if (!SeenExtensionBindingKeys.insert(Key).second)
            {
                FDiagnostic Diagnostic = MakePackageDiagnostic(
                    Descriptor,
                    DuplicateExtensionSchemaBindingCode,
                    "Package contains more than one exact extension schema binding",
                    Binding.GetRelativePath());
                Diagnostic.SchemaId = Binding.GetSchemaId();
                Diagnostic.SchemaVersion = Binding.GetSchemaVersion();
                Diagnostics.push_back(std::move(Diagnostic));
            }
        }

        std::set<std::string> ClaimedRetiredIds;
        for (const FRedirectDescriptor& Redirect : Descriptor.GetRedirects())
        {
            FStableIdView Source;
            FStableIdView Target;
            const bool bSourceValid = FStableId::Parse(Redirect.GetSourceId(), Source);
            const bool bTargetValid = FStableId::Parse(Redirect.GetTargetId(), Target);
            if (!bSourceValid || !bTargetValid)
            {
                FDiagnostic Diagnostic = MakePackageDiagnostic(
                    Descriptor, InvalidRedirectCode,
                    "Redirect source and target must be canonical Stable IDs");
                Diagnostic.DefinitionId = Redirect.GetSourceId();
                Diagnostics.push_back(std::move(Diagnostic));
                continue;
            }
            if (Source.Namespace != Namespace)
            {
                FDiagnostic Diagnostic = MakePackageDiagnostic(
                    Descriptor, ForeignRedirectCode,
                    "Redirect source must be owned by the declaring package namespace");
                Diagnostic.DefinitionId = Redirect.GetSourceId();
                Diagnostics.push_back(std::move(Diagnostic));
            }
            if (Source.Kind != Target.Kind)
            {
                FDiagnostic Diagnostic = MakePackageDiagnostic(
                    Descriptor, RedirectKindMismatchCode,
                    "Redirect source and target must have the same Stable ID kind");
                Diagnostic.DefinitionId = Redirect.GetSourceId();
                Diagnostics.push_back(std::move(Diagnostic));
            }
            if (!ClaimedRetiredIds.insert(Redirect.GetSourceId()).second)
            {
                FDiagnostic Diagnostic = MakePackageDiagnostic(
                    Descriptor, RedirectConflictCode,
                    "A redirect source may have only one target");
                Diagnostic.DefinitionId = Redirect.GetSourceId();
                Diagnostics.push_back(std::move(Diagnostic));
            }
        }
        for (const std::string& Tombstone : Descriptor.GetTombstones())
        {
            FStableIdView Parsed;
            if (!FStableId::Parse(Tombstone, Parsed))
            {
                FDiagnostic Diagnostic = MakePackageDiagnostic(
                    Descriptor, InvalidTombstoneCode,
                    "Tombstone must be a canonical Stable ID");
                Diagnostic.DefinitionId = Tombstone;
                Diagnostics.push_back(std::move(Diagnostic));
                continue;
            }
            if (Parsed.Namespace != Namespace)
            {
                FDiagnostic Diagnostic = MakePackageDiagnostic(
                    Descriptor, ForeignTombstoneCode,
                    "Tombstone ID must be owned by the declaring package namespace");
                Diagnostic.DefinitionId = Tombstone;
                Diagnostics.push_back(std::move(Diagnostic));
            }
            if (!ClaimedRetiredIds.insert(Tombstone).second)
            {
                FDiagnostic Diagnostic = MakePackageDiagnostic(
                    Descriptor, RedirectConflictCode,
                    "An ID cannot be declared by multiple redirects or tombstones");
                Diagnostic.DefinitionId = Tombstone;
                Diagnostics.push_back(std::move(Diagnostic));
            }
        }
    }

    std::map<std::string, const FPackageDescriptor*> PackageMap;
    for (const FPackageDescriptor* DescriptorPointer : OrderedDescriptors)
    {
        PackageMap[DescriptorPointer->GetPackageId()] = DescriptorPointer;
    }

    // PKG-06: Dependency existence and load_after ordering checks
    for (const FPackageDescriptor* DescriptorPointer : OrderedDescriptors)
    {
        const FPackageDescriptor& Descriptor = *DescriptorPointer;
        for (const FPackageDependency& Dep : Descriptor.GetDependencies())
        {
            const auto TargetIt = PackageMap.find(Dep.GetPackageId());
            if (TargetIt == PackageMap.end())
            {
                Diagnostics.push_back(MakePackageDiagnostic(
                    Descriptor,
                    MissingDependencyCode,
                    "Package '" + Descriptor.GetPackageId() + "' requires missing dependency '" + Dep.GetPackageId() + "'"));
            }
            else
            {
                const FPackageDescriptor& TargetDesc = *TargetIt->second;
                if (Dep.GetLoadAfter() && Descriptor.GetLoadIndex() < TargetDesc.GetLoadIndex())
                {
                    Diagnostics.push_back(MakePackageDiagnostic(
                        Descriptor,
                        LoadAfterViolationCode,
                        "Package '" + Descriptor.GetPackageId() + "' (load_index "
                            + std::to_string(Descriptor.GetLoadIndex())
                            + ") specifies load_after on '" + TargetDesc.GetPackageId()
                            + "' (load_index " + std::to_string(TargetDesc.GetLoadIndex())
                            + "), but is ordered before it"));
                }
            }
        }
    }

    // PKG-06: Dependency cycle detection (DFS 3-color graph traversal)
    std::map<std::string, std::vector<std::string>> DependencyGraph;
    for (const FPackageDescriptor* DescriptorPointer : OrderedDescriptors)
    {
        const std::string& SourceId = DescriptorPointer->GetPackageId();
        for (const FPackageDependency& Dep : DescriptorPointer->GetDependencies())
        {
            if (PackageMap.find(Dep.GetPackageId()) != PackageMap.end())
            {
                DependencyGraph[SourceId].push_back(Dep.GetPackageId());
            }
        }
    }

    std::map<std::string, int> VisitedState; // 0 = unvisited, 1 = visiting, 2 = visited
    std::set<std::string> EmittedCycleSources;

    std::function<void(const std::string&, std::vector<std::string>&)> DetectCycles =
        [&](const std::string& Node, std::vector<std::string>& Stack)
    {
        VisitedState[Node] = 1;
        Stack.push_back(Node);

        if (auto AdjIt = DependencyGraph.find(Node); AdjIt != DependencyGraph.end())
        {
            for (const std::string& Neighbor : AdjIt->second)
            {
                if (VisitedState[Neighbor] == 1)
                {
                    auto CycleStartIt = std::find(Stack.begin(), Stack.end(), Neighbor);
                    if (CycleStartIt != Stack.end())
                    {
                        std::string CyclePath;
                        for (auto It = CycleStartIt; It != Stack.end(); ++It)
                        {
                            if (!CyclePath.empty())
                            {
                                CyclePath += " -> ";
                            }
                            CyclePath += *It;
                        }
                        CyclePath += " -> " + Neighbor;

                        if (EmittedCycleSources.insert(Node).second)
                        {
                            auto DescIt = PackageMap.find(Node);
                            if (DescIt != PackageMap.end())
                            {
                                Diagnostics.push_back(MakePackageDiagnostic(
                                    *DescIt->second,
                                    DependencyCycleCode,
                                    "Dependency cycle detected: " + CyclePath));
                            }
                        }
                    }
                }
                else if (VisitedState[Neighbor] == 0)
                {
                    DetectCycles(Neighbor, Stack);
                }
            }
        }

        Stack.pop_back();
        VisitedState[Node] = 2;
    };

    for (const auto& [PkgId, _] : PackageMap)
    {
        if (VisitedState[PkgId] == 0)
        {
            std::vector<std::string> Stack;
            DetectCycles(PkgId, Stack);
        }
    }

    if (!bHasCoreAtZero)
    {
        FDiagnostic Diagnostic;
        Diagnostic.Code = MissingCoreCode;
        Diagnostic.Message = "Package set is missing required core package at load_index 0";
        Diagnostics.push_back(std::move(Diagnostic));
    }

    std::sort(Diagnostics.begin(), Diagnostics.end());
    return Diagnostics;
}
}
