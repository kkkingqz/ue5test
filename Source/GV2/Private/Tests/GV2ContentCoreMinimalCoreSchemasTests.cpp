#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/RepositoryBuilder.h"
#include "GV2ContentCore/Testing/RepresentativeCore.h"
#include "Containers/StringConv.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include <map>
#include <set>

namespace
{
class FMinimalCoreFixtureProvider final : public GV2ContentCore::IContentSourceProvider
{
public:
    explicit FMinimalCoreFixtureProvider(FString InRoot) : Root(std::move(InRoot)) {}

    std::optional<std::string> ReadSource(
        const std::string_view PackageId,
        const std::string_view RelativeSource) const override
    {
        const FString Package = UTF8_TO_TCHAR(std::string(PackageId).c_str());
        const FString Relative = UTF8_TO_TCHAR(std::string(RelativeSource).c_str());
        TArray<uint8> FileBytes;
        if (!FFileHelper::LoadFileToArray(FileBytes, *FPaths::Combine(Root, Package, Relative)))
        {
            return std::nullopt;
        }
        return std::string(reinterpret_cast<const char*>(FileBytes.GetData()), FileBytes.Num());
    }

private:
    FString Root;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2ContentCoreMinimalCoreSchemasTest,
    "GV2.Runtime.ContentCore.MinimalCoreSchemas",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2ContentCoreMinimalCoreSchemasTest::RunTest(const FString& Parameters)
{
    using namespace GV2ContentCore;

    const FString FixtureRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(), TEXT("Tests/Fixtures/PortableContentCore/valid")));
    const FMinimalCoreFixtureProvider Provider(FixtureRoot);
    FBuildOptions Options;
    Options.SourceProvider = &Provider;
    const FBuildResult Result = BuildRepository(
        { Testing::MakeRepresentativeCorePackageDescriptor() }, Options);
    TestTrue(TEXT("representative core package passes complete M3 validation"), Result.IsSuccess());
    if (!Result.IsSuccess())
    {
        for (const FDiagnostic& Diagnostic : Result.GetDiagnostics())
        {
            AddError(UTF8_TO_TCHAR((Diagnostic.Code + ": " + Diagnostic.Message).c_str()));
        }
        return false;
    }

    const FValue* Definitions = Result.GetCandidate().GetRootValue().FindField("definitions");
    TestTrue(TEXT("M3 candidate contains normalized definitions"),
        Definitions != nullptr && Definitions->IsArray() && !Definitions->AsArray().empty());
    if (Definitions == nullptr || !Definitions->IsArray()) return false;

    // TAS-10: kind membership and uniqueness are the real invariants, not a
    // pinned total/per-kind count — the frozen corpus (TAS-06) may still
    // gain a definition when the subject of the change is content-
    // resolution rules themselves.
    std::map<std::string, std::size_t> KindCounts;
    std::set<std::string> SeenIds;
    const FValue* Item = nullptr;
    bool bNoDuplicateIds = true;
    for (const FValue& Definition : Definitions->AsArray())
    {
        ++KindCounts[Definition.FindField("type")->AsString()];
        const std::string Id = Definition.FindField("id")->AsString();
        if (!SeenIds.insert(Id).second) bNoDuplicateIds = false;
        if (Id == "core:item.weapon.iron_sword") Item = &Definition;
    }
    TestTrue(TEXT("every definition id is unique"), bNoDuplicateIds);
    TestTrue(TEXT("at least one location"), KindCounts["location"] >= 1);
    TestTrue(TEXT("at least one screen"), KindCounts["screen"] >= 1);
    TestTrue(TEXT("at least one item"), KindCounts["item"] >= 1);
    TestTrue(TEXT("at least one text"), KindCounts["text"] >= 1);
    TestTrue(TEXT("at least one resource"), KindCounts["resource"] >= 1);
    TestTrue(TEXT("at least one actor"), KindCounts["actor"] >= 1);
    // Kind coverage is genuinely the subject here — this corpus exists to
    // exercise exactly these six schemas end to end. A seventh kind means
    // either a new schema needs its own coverage decision or an
    // accidental leak; either way this must fail loudly, not silently pass.
    TestTrue(TEXT("no future gameplay kinds were added"), KindCounts.size() == 6);
    TestTrue(TEXT("item metadata survives candidate materialization"), Item != nullptr
        && Item->FindField("tags")->AsArray().size() == 2
        && !Item->FindField("deprecated")->AsBoolean()
        && Item->FindField("extensions")->IsObject());
    return true;
}

#endif
