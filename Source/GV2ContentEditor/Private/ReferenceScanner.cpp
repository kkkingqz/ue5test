#include "GV2ContentEditor/ReferenceScanner.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ParseLimits.h"
#include "GV2ContentCore/StableId.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>

namespace GV2ContentEditor
{

namespace
{

bool ReadFileContent(const std::filesystem::path& Path, std::string& OutContent)
{
    std::ifstream Stream(Path, std::ios::binary);
    if (!Stream.is_open()) return false;
    OutContent.assign(
        std::istreambuf_iterator<char>(Stream),
        std::istreambuf_iterator<char>());
    return true;
}

std::string EscapeJsonPointerSegment(std::string_view Token)
{
    std::string Escaped;
    for (const char Character : Token)
    {
        if (Character == '~')
        {
            Escaped += "~0";
        }
        else if (Character == '/')
        {
            Escaped += "~1";
        }
        else
        {
            Escaped.push_back(Character);
        }
    }
    return Escaped;
}

void ScanValueForOutgoingRefs(
    const GV2ContentCore::FValue& Value,
    const std::string& CurrentPointer,
    const std::string& SourceDefId,
    const std::string& RelativeSource,
    std::vector<FGV2ReferenceItem>& OutItems)
{
    if (Value.IsString())
    {
        const std::string& Str = Value.AsString();
        GV2ContentCore::FStableIdView IdView;
        if (GV2ContentCore::FStableId::Parse(Str, IdView))
        {
            // Do not treat self-identity as an outgoing reference
            if (Str != SourceDefId)
            {
                FGV2ReferenceItem Item;
                Item.SourceDefinitionId = SourceDefId;
                Item.TargetDefinitionId = Str;
                Item.TargetKind = std::string(IdView.Kind);
                Item.RelativeSource = RelativeSource;
                Item.JsonPointer = CurrentPointer;
                OutItems.push_back(std::move(Item));
            }
        }
        return;
    }
    if (Value.IsArray())
    {
        const auto& Arr = Value.AsArray();
        for (std::size_t i = 0; i < Arr.size(); ++i)
        {
            ScanValueForOutgoingRefs(Arr[i], CurrentPointer + "/" + std::to_string(i), SourceDefId, RelativeSource, OutItems);
        }
        return;
    }
    if (Value.IsObject())
    {
        const auto& Obj = Value.AsObject();
        for (const auto& [Key, ChildVal] : Obj)
        {
            ScanValueForOutgoingRefs(ChildVal, CurrentPointer + "/" + EscapeJsonPointerSegment(Key), SourceDefId, RelativeSource, OutItems);
        }
        return;
    }
}

void ScanValueForIncomingRefs(
    const GV2ContentCore::FValue& Value,
    const std::string& CurrentPointer,
    const std::string& RelativeSource,
    const std::string& CurrentDefId,
    const std::string& TargetId,
    const GV2ContentCore::FParsedDocument& Doc,
    std::vector<FGV2ReferenceItem>& OutItems)
{
    if (Value.IsString())
    {
        if (Value.AsString() == TargetId)
        {
            FGV2ReferenceItem Item;
            Item.SourceDefinitionId = CurrentDefId;
            Item.TargetDefinitionId = TargetId;
            Item.RelativeSource = RelativeSource;
            Item.JsonPointer = CurrentPointer;
            if (const auto* Loc = Doc.FindLocation(CurrentPointer))
            {
                Item.Line = Loc->ValueSpan.StartLine;
                Item.Column = Loc->ValueSpan.StartColumn;
            }
            OutItems.push_back(std::move(Item));
        }
        return;
    }
    if (Value.IsArray())
    {
        const auto& Arr = Value.AsArray();
        for (std::size_t Index = 0; Index < Arr.size(); ++Index)
        {
            const std::string ChildPointer = CurrentPointer + "/" + std::to_string(Index);
            std::string DefId = CurrentDefId;
            if (CurrentPointer == "/definitions" && Arr[Index].IsObject())
            {
                const auto& Obj = Arr[Index].AsObject();
                for (const auto& [Key, FieldVal] : Obj)
                {
                    if (Key == "id" && FieldVal.IsString())
                    {
                        DefId = FieldVal.AsString();
                        break;
                    }
                }
            }
            ScanValueForIncomingRefs(Arr[Index], ChildPointer, RelativeSource, DefId, TargetId, Doc, OutItems);
        }
        return;
    }
    if (Value.IsObject())
    {
        const auto& Obj = Value.AsObject();
        for (const auto& [Key, ChildVal] : Obj)
        {
            const std::string ChildPointer = CurrentPointer + "/" + EscapeJsonPointerSegment(Key);
            if (Key == "id" && !CurrentDefId.empty() && CurrentPointer.rfind("/definitions/", 0) == 0 && CurrentPointer.find('/', 13) == std::string::npos)
            {
                continue;
            }
            ScanValueForIncomingRefs(ChildVal, ChildPointer, RelativeSource, CurrentDefId, TargetId, Doc, OutItems);
        }
        return;
    }
}

} // namespace

std::vector<FGV2ReferenceItem> FGV2ReferenceScanner::FindOutgoingReferences(
    const FGV2LoadedDefinition& LoadedDef,
    const FGV2SchemaFormModel* /*FormModel*/)
{
    std::vector<FGV2ReferenceItem> Out;
    ScanValueForOutgoingRefs(LoadedDef.CanonicalData, "/data", LoadedDef.Id, LoadedDef.RelativeSource, Out);
    if (!LoadedDef.Extensions.IsNull())
    {
        ScanValueForOutgoingRefs(LoadedDef.Extensions, "/extensions", LoadedDef.Id, LoadedDef.RelativeSource, Out);
    }

    std::sort(Out.begin(), Out.end(), [](const FGV2ReferenceItem& A, const FGV2ReferenceItem& B) {
        if (A.TargetDefinitionId != B.TargetDefinitionId) return A.TargetDefinitionId < B.TargetDefinitionId;
        return A.JsonPointer < B.JsonPointer;
    });

    return Out;
}

std::vector<FGV2ReferenceItem> FGV2ReferenceScanner::FindIncomingReferences(
    const std::string& TargetDefinitionId,
    const std::vector<FGV2DefinitionSummary>& /*AllDefinitions*/,
    const std::vector<std::filesystem::path>& PackageRoots,
    const std::vector<GV2ContentCore::FPackageDescriptor>& Packages)
{
    std::vector<FGV2ReferenceItem> Out;
    GV2ContentCore::FParseLimits Limits;

    for (std::size_t PkgIdx = 0; PkgIdx < Packages.size(); ++PkgIdx)
    {
        const auto& Pkg = Packages[PkgIdx];
        const auto& Root = PackageRoots[PkgIdx];

        for (const std::string& RelSource : Pkg.GetRelativeSources())
        {
            std::filesystem::path FilePath = Root / RelSource;
            std::string Content;
            if (!ReadFileContent(FilePath, Content)) continue;

            std::vector<GV2ContentCore::FDiagnostic> Diags;
            auto Doc = GV2ContentCore::ParseJson5Document(Content, Limits, Diags, Pkg.GetPackageId(), 0, RelSource);
            if (Doc.has_value())
            {
                ScanValueForIncomingRefs(Doc->GetRootValue(), "", RelSource, "", TargetDefinitionId, *Doc, Out);
            }
        }

        for (const auto& Redirect : Pkg.GetRedirects())
        {
            if (Redirect.GetTargetId() == TargetDefinitionId)
            {
                FGV2ReferenceItem Item;
                Item.SourceDefinitionId = Redirect.GetSourceId();
                Item.TargetDefinitionId = TargetDefinitionId;
                Item.RelativeSource = "package.json5";
                Item.JsonPointer = "/redirects/" + EscapeJsonPointerSegment(Redirect.GetSourceId());
                Out.push_back(std::move(Item));
            }
        }
    }

    std::sort(Out.begin(), Out.end(), [](const FGV2ReferenceItem& A, const FGV2ReferenceItem& B) {
        if (A.RelativeSource != B.RelativeSource) return A.RelativeSource < B.RelativeSource;
        if (A.Line != B.Line) return A.Line < B.Line;
        if (A.Column != B.Column) return A.Column < B.Column;
        return A.JsonPointer < B.JsonPointer;
    });

    return Out;
}

std::vector<std::string> FGV2ReferenceScanner::FindCompatibleTargets(
    const std::string& ExpectedKind,
    const std::vector<FGV2DefinitionSummary>& AllDefinitions)
{
    std::set<std::string> Targets;
    for (const auto& Def : AllDefinitions)
    {
        GV2ContentCore::FStableIdView IdView;
        if (GV2ContentCore::FStableId::Parse(Def.Id, IdView))
        {
            if (ExpectedKind.empty() || ExpectedKind == "*" || IdView.Kind == ExpectedKind || Def.Type == ExpectedKind)
            {
                Targets.insert(Def.Id);
            }
        }
    }
    return std::vector<std::string>(Targets.begin(), Targets.end());
}

} // namespace GV2ContentEditor
