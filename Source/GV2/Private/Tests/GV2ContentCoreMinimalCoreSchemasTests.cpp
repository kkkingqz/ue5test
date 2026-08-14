#if WITH_DEV_AUTOMATION_TESTS

#include "GV2ContentCore/RepositoryBuilder.h"
#include "GV2ContentCore/Testing/RepresentativeCore.h"
#include "Containers/StringConv.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include <map>

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
        FString Source;
        if (!FFileHelper::LoadFileToString(Source, *FPaths::Combine(Root, Package, Relative)))
        {
            return std::nullopt;
        }
        FTCHARToUTF8 Utf8(*Source);
        return std::string(Utf8.Get(), Utf8.Length());
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
        Definitions != nullptr && Definitions->IsArray());
    if (Definitions == nullptr || !Definitions->IsArray()) return false;
    TestEqual(TEXT("representative core contains nine definitions"),
        Definitions->AsArray().size(), static_cast<std::size_t>(9));

    std::map<std::string, std::size_t> KindCounts;
    const FValue* Item = nullptr;
    for (const FValue& Definition : Definitions->AsArray())
    {
        ++KindCounts[Definition.FindField("type")->AsString()];
        if (Definition.FindField("id")->AsString() == "core:item.weapon.iron_sword") Item = &Definition;
    }
    TestEqual(TEXT("one location"), KindCounts["location"], static_cast<std::size_t>(1));
    TestEqual(TEXT("two screens"), KindCounts["screen"], static_cast<std::size_t>(2));
    TestEqual(TEXT("one item"), KindCounts["item"], static_cast<std::size_t>(1));
    TestEqual(TEXT("four text definitions"), KindCounts["text"], static_cast<std::size_t>(4));
    TestEqual(TEXT("one resource definition"), KindCounts["resource"], static_cast<std::size_t>(1));
    TestTrue(TEXT("no future gameplay kinds were added"), KindCounts.size() == 5);
    TestTrue(TEXT("item metadata survives candidate materialization"), Item != nullptr
        && Item->FindField("tags")->AsArray().size() == 2
        && !Item->FindField("deprecated")->AsBoolean()
        && Item->FindField("extensions")->IsObject());
    return true;
}

#endif
