#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Application/GV2ScreenFieldMaterializer.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ParseLimits.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2ImageWidgetBase.h"
#include "UI/GV2ListViewWidgetBase.h"
#include "UI/GV2UiTheme.h"

#include "Tests/GV2PresentationTestFixtures.h"

using namespace GV2PresentationTestFixtures;
using namespace GV2SyntheticMechanicalFixture;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SyntheticMechanicalFixtureContractTest,
    "GV2.Runtime.Presentation.SyntheticMechanicalFixtureContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SyntheticMechanicalFixtureContractTest::RunTest(const FString& Parameters)
{
    // =========================================================================
    // 1. Schema Cache Compilation of Synthetic Schemas under core:
    // =========================================================================
    {
        const FString SyntheticSchemaDir = GetSyntheticSchemaDir();
        TestTrue(
            TEXT("Synthetic schema directory exists"),
            FPaths::DirectoryExists(SyntheticSchemaDir));

        TArray<FGV2SchemaPackageRoot> TestRoots = {
            FGV2SchemaPackageRoot{ TEXT("core"), SyntheticSchemaDir }
        };

        TSharedPtr<FGV2UiSchemaCache> Cache = FGV2UiSchemaCacheTestAccess::Create(TestRoots);
        TestNotNull(TEXT("FGV2UiSchemaCache instance created"), Cache.Get());

        if (Cache.IsValid())
        {
            FString CompileError;
            const bool bCompiled = FGV2UiSchemaCacheTestAccess::CompileAll(*Cache, CompileError);
            TestTrue(
                *FString::Printf(TEXT("CompileAll succeeds on synthetic schemas [Error: %s]"), *CompileError),
                bCompiled);

            // Verify each synthetic field schema is compiled and discoverable
            const std::string SceneSchemaIdUtf8 = TCHAR_TO_UTF8(SceneSchemaId);
            const std::string CommandsSchemaIdUtf8 = TCHAR_TO_UTF8(CommandsSchemaId);
            const std::string TopBarSchemaIdUtf8 = TCHAR_TO_UTF8(TopBarSchemaId);
            const std::string StatusSchemaIdUtf8 = TCHAR_TO_UTF8(PlayerStatusSchemaId);

            FString SpecError;
            TestNotNull(
                TEXT("Synthetic scene schema compiled"),
                Cache->GetCompiledSchema(SceneSchemaIdUtf8, SpecError).get());
            TestNotNull(
                TEXT("Synthetic commands schema compiled"),
                Cache->GetCompiledSchema(CommandsSchemaIdUtf8, SpecError).get());
            TestNotNull(
                TEXT("Synthetic top bar schema compiled"),
                Cache->GetCompiledSchema(TopBarSchemaIdUtf8, SpecError).get());
            TestNotNull(
                TEXT("Synthetic player status schema compiled"),
                Cache->GetCompiledSchema(StatusSchemaIdUtf8, SpecError).get());
        }
    }

    // =========================================================================
    // 2. Production Path Resolution via PrepareBindingDefinitions & BuildFields
    // =========================================================================
    {
        UGV2UiTheme* Theme = LoadConfiguredThemeForTest();
        TestNotNull(TEXT("Configured UI theme is valid"), Theme);
        FScopedSyntheticFallbackTexts ScopedTexts(Theme);

        FSyntheticPrepareContextFixture PrepareFixture;
        FString InitError;
        const bool bInitOk = PrepareFixture.Initialize(InitError);
        TestTrue(
            *FString::Printf(TEXT("Synthetic prepare context initialized [Error: %s]"), *InitError),
            bInitOk);

        const FGV2PresentationPrepareContext* PrepareContext = PrepareFixture.Get();
        if (bInitOk && PrepareContext != nullptr)
        {
            // --- 2a. Location Alpha Request (1 character, travel to beta) ---
            const GV2RuntimeCore::FScreenRequest AlphaReq = CreateLocationAlphaScreenRequest();
            TArray<FGV2UiBindingDefinition> AlphaDefs;
            const bool bAlphaBindingOk = GV2ScreenFieldMaterializer::PrepareBindingDefinitions(
                *PrepareContext, AlphaReq, AlphaDefs);
            TestTrue(TEXT("Alpha ScreenRequest PrepareBindingDefinitions succeeds"), bAlphaBindingOk);
            TestEqual(TEXT("Alpha ScreenRequest produces exactly 1 binding definition"), AlphaDefs.Num(), 1);
            if (AlphaDefs.Num() == 1)
            {
                TestEqual(
                    TEXT("Alpha travel command ID matches expected"),
                    AlphaDefs[0].CommandId,
                    TravelBetaCommandId);
            }

            TArray<FGV2UiBindingHandle> AlphaHandles = {
                FGV2UiBindingHandle::Create(TravelBetaCommandId)
            };
            TArray<FGV2ScreenFieldValue> AlphaFields;
            const bool bAlphaFieldsOk = GV2ScreenFieldMaterializer::BuildFields(
                *PrepareContext, AlphaReq, AlphaHandles, AlphaFields);
            TestTrue(TEXT("Alpha ScreenRequest BuildFields succeeds"), bAlphaFieldsOk);
            TestEqual(TEXT("Alpha produces 4 fields (scene, commands, top_bar, player_status)"), AlphaFields.Num(), 4);

            // Find scene field in Alpha and verify 1 character
            const FGV2ScreenFieldValue* AlphaSceneField = AlphaFields.FindByPredicate(
                [](const FGV2ScreenFieldValue& F) { return F.FieldId == FName(TEXT("scene")); });
            TestNotNull(TEXT("Alpha contains scene field"), AlphaSceneField);
            if (AlphaSceneField != nullptr && AlphaSceneField->PreparedValue.IsValid())
            {
                const FGV2PreparedUiValue* CharsVal = AlphaSceneField->PreparedValue->FindField(TEXT("characters"));
                TestNotNull(TEXT("Scene contains characters array"), CharsVal);
                if (CharsVal != nullptr && CharsVal->IsArray())
                {
                    TestEqual(TEXT("Alpha scene has exactly 1 character"), CharsVal->AsArray().Num(), 1);
                    if (CharsVal->AsArray().Num() == 1 && CharsVal->AsArray()[0].IsObject())
                    {
                        const FGV2PreparedUiObject& CharObj = CharsVal->AsArray()[0].AsObject();
                        const FGV2PreparedUiValue* KeyVal = CharObj.FindField(TEXT("key"));
                        TestNotNull(TEXT("Character has key"), KeyVal);
                        if (KeyVal != nullptr)
                        {
                            TestEqual(TEXT("Character key is guide"), KeyVal->AsKey(), CharacterKey.ToString());
                        }
                    }
                }
            }

            // Find commands field in Alpha and verify travel_beta button
            const FGV2ScreenFieldValue* AlphaCommandsField = AlphaFields.FindByPredicate(
                [](const FGV2ScreenFieldValue& F) { return F.FieldId == FName(TEXT("commands")); });
            TestNotNull(TEXT("Alpha contains commands field"), AlphaCommandsField);
            if (AlphaCommandsField != nullptr && AlphaCommandsField->PreparedValue.IsValid())
            {
                const FGV2PreparedUiValue* ItemsVal = AlphaCommandsField->PreparedValue->FindField(TEXT("items"));
                TestNotNull(TEXT("Commands contains items array"), ItemsVal);
                if (ItemsVal != nullptr && ItemsVal->IsArray())
                {
                    TestEqual(TEXT("Alpha commands has exactly 1 item"), ItemsVal->AsArray().Num(), 1);
                    if (ItemsVal->AsArray().Num() == 1 && ItemsVal->AsArray()[0].IsObject())
                    {
                        const FGV2PreparedUiObject& ItemObj = ItemsVal->AsArray()[0].AsObject();
                        const FGV2PreparedUiValue* KeyVal = ItemObj.FindField(TEXT("key"));
                        TestNotNull(TEXT("Command item has key"), KeyVal);
                        if (KeyVal != nullptr)
                        {
                            TestEqual(TEXT("Command key is travel_beta"), KeyVal->AsKey(), TravelBetaKey.ToString());
                        }
                    }
                }
            }

            // --- 2b. Location Beta Request (0 characters, travel to alpha) ---
            const GV2RuntimeCore::FScreenRequest BetaReq = CreateLocationBetaScreenRequest();
            TArray<FGV2UiBindingDefinition> BetaDefs;
            const bool bBetaBindingOk = GV2ScreenFieldMaterializer::PrepareBindingDefinitions(
                *PrepareContext, BetaReq, BetaDefs);
            TestTrue(TEXT("Beta ScreenRequest PrepareBindingDefinitions succeeds"), bBetaBindingOk);
            TestEqual(TEXT("Beta ScreenRequest produces exactly 1 binding definition"), BetaDefs.Num(), 1);
            if (BetaDefs.Num() == 1)
            {
                TestEqual(
                    TEXT("Beta travel command ID matches expected"),
                    BetaDefs[0].CommandId,
                    TravelAlphaCommandId);
            }

            TArray<FGV2UiBindingHandle> BetaHandles = {
                FGV2UiBindingHandle::Create(TravelAlphaCommandId)
            };
            TArray<FGV2ScreenFieldValue> BetaFields;
            const bool bBetaFieldsOk = GV2ScreenFieldMaterializer::BuildFields(
                *PrepareContext, BetaReq, BetaHandles, BetaFields);
            TestTrue(TEXT("Beta ScreenRequest BuildFields succeeds"), bBetaFieldsOk);
            TestEqual(TEXT("Beta produces 4 fields"), BetaFields.Num(), 4);

            // Find scene field in Beta and verify 0 characters
            const FGV2ScreenFieldValue* BetaSceneField = BetaFields.FindByPredicate(
                [](const FGV2ScreenFieldValue& F) { return F.FieldId == FName(TEXT("scene")); });
            TestNotNull(TEXT("Beta contains scene field"), BetaSceneField);
            if (BetaSceneField != nullptr && BetaSceneField->PreparedValue.IsValid())
            {
                const FGV2PreparedUiValue* CharsVal = BetaSceneField->PreparedValue->FindField(TEXT("characters"));
                TestNotNull(TEXT("Beta scene contains characters array"), CharsVal);
                if (CharsVal != nullptr && CharsVal->IsArray())
                {
                    TestEqual(TEXT("Beta scene has exactly 0 characters"), CharsVal->AsArray().Num(), 0);
                }
            }

            // Find commands field in Beta and verify travel_alpha button (and no travel_beta)
            const FGV2ScreenFieldValue* BetaCommandsField = BetaFields.FindByPredicate(
                [](const FGV2ScreenFieldValue& F) { return F.FieldId == FName(TEXT("commands")); });
            TestNotNull(TEXT("Beta contains commands field"), BetaCommandsField);
            if (BetaCommandsField != nullptr && BetaCommandsField->PreparedValue.IsValid())
            {
                const FGV2PreparedUiValue* ItemsVal = BetaCommandsField->PreparedValue->FindField(TEXT("items"));
                TestNotNull(TEXT("Beta commands contains items array"), ItemsVal);
                if (ItemsVal != nullptr && ItemsVal->IsArray())
                {
                    TestEqual(TEXT("Beta commands has exactly 1 item"), ItemsVal->AsArray().Num(), 1);
                    if (ItemsVal->AsArray().Num() == 1 && ItemsVal->AsArray()[0].IsObject())
                    {
                        const FGV2PreparedUiObject& ItemObj = ItemsVal->AsArray()[0].AsObject();
                        const FGV2PreparedUiValue* KeyVal = ItemObj.FindField(TEXT("key"));
                        TestNotNull(TEXT("Command item has key"), KeyVal);
                        if (KeyVal != nullptr)
                        {
                            TestEqual(TEXT("Command key is travel_alpha"), KeyVal->AsKey(), TravelAlphaKey.ToString());
                            TestFalse(TEXT("Command key is NOT travel_beta"), KeyVal->AsKey() == TravelBetaKey.ToString());
                        }
                    }
                }
            }
        }
    }

    // =========================================================================
    // 3. Structural Differentiation from GameData/rh/definitions/locations.json5
    // =========================================================================
    {
        const FString RhLocationsPath = FPaths::Combine(
            FPaths::ProjectDir(), TEXT("GameData/rh/definitions/locations.json5"));
        FString RhJsonContent;
        TestTrue(
            TEXT("Can read GameData/rh/definitions/locations.json5"),
            FFileHelper::LoadFileToString(RhJsonContent, *RhLocationsPath));

        std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
        std::optional<GV2ContentCore::FParsedDocument> RhDoc = GV2ContentCore::ParseJson5Document(
            TCHAR_TO_UTF8(*RhJsonContent), GV2ContentCore::FParseLimits{}, Diagnostics);
        TestTrue(TEXT("Parsed GameData/rh/definitions/locations.json5"), RhDoc.has_value());

        if (RhDoc.has_value())
        {
            const GV2ContentCore::FValue* RhDefsVal = RhDoc->GetRootValue().FindField("definitions");
            TestNotNull(TEXT("RH definitions array present"), RhDefsVal);
            if (RhDefsVal != nullptr && RhDefsVal->IsArray())
            {
                const int32 RhLocationCount = static_cast<int32>(RhDefsVal->AsArray().size());
                // RH has 3 locations: market, tavern, gate
                TestEqual(TEXT("RH contains exactly 3 locations"), RhLocationCount, 3);

                // Synthetic fixture has exactly 2 locations: alpha, beta
                const int32 SyntheticLocationCount = 2;
                TestFalse(
                    TEXT("Synthetic location count (2) strictly differs from RH location count (3)"),
                    SyntheticLocationCount == RhLocationCount);

                // Check that none of RH definitions share IDs with synthetic locations
                TArray<FString> RhIds;
                for (const auto& DefItem : RhDefsVal->AsArray())
                {
                    if (DefItem.IsObject())
                    {
                        if (const auto* IdVal = DefItem.FindField("id"); IdVal && IdVal->IsString())
                        {
                            RhIds.Add(UTF8_TO_TCHAR(IdVal->AsString().c_str()));
                        }
                    }
                }
                TestFalse(TEXT("RH IDs do not contain LocationAlphaId"), RhIds.Contains(LocationAlphaId));
                TestFalse(TEXT("RH IDs do not contain LocationBetaId"), RhIds.Contains(LocationBetaId));
            }
        }
    }

    // =========================================================================
    // 4. Isolation from Production Host Package Discovery & mods.lock.json5
    // =========================================================================
    {
        const FString GameDataDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData"));
        std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
        const std::optional<GV2ContentHostSupport::FResolvedPackageSet> DiscoveredSet =
            GV2ContentHostSupport::ResolvePackageSetFromContainer(
                std::filesystem::path(TCHAR_TO_UTF8(*GameDataDir)), Diagnostics);

        TestTrue(TEXT("Production package discovery resolves from GameData container"), DiscoveredSet.has_value());
        if (DiscoveredSet.has_value())
        {
            TArray<FString> DiscoveredPackageIds;
            for (const auto& Src : DiscoveredSet->OrderedSources)
            {
                DiscoveredPackageIds.Add(UTF8_TO_TCHAR(Src.Descriptor.GetPackageId().c_str()));
            }

            TestFalse(
                TEXT("Synthetic fixture is NOT discovered as a production package"),
                DiscoveredPackageIds.Contains(TEXT("SyntheticMechanicalFixture")));
            TestFalse(
                TEXT("No synthetic package ID present in production discovery"),
                DiscoveredPackageIds.Contains(TEXT("synthetic")));
        }

        // Check mods.lock.json5
        const FString ModsLockPath = FPaths::Combine(GameDataDir, TEXT("mods.lock.json5"));
        FString ModsLockContent;
        if (FFileHelper::LoadFileToString(ModsLockContent, *ModsLockPath))
        {
            TestFalse(
                TEXT("mods.lock.json5 does not mention SyntheticMechanicalFixture"),
                ModsLockContent.Contains(TEXT("SyntheticMechanicalFixture")));
            TestFalse(
                TEXT("mods.lock.json5 does not mention synthetic"),
                ModsLockContent.Contains(TEXT("synthetic")));
        }
    }

    // =========================================================================
    // 5. STATUS-026 Invariant: Fixture is NOT in core:module.bootstrap.main Graph
    // =========================================================================
    {
        const FString BootstrapManifestPath = FPaths::Combine(
            FPaths::ProjectDir(), TEXT("Scripts/bootstrap/manifest.lua"));
        FString ManifestContent;
        if (TestTrue(
                TEXT("Scripts/bootstrap/manifest.lua exists and is readable"),
                FFileHelper::LoadFileToString(ManifestContent, *BootstrapManifestPath)))
        {
            TestFalse(
                TEXT("Bootstrap manifest does not declare SyntheticMechanicalFixture"),
                ManifestContent.Contains(TEXT("SyntheticMechanicalFixture")));
            TestFalse(
                TEXT("Bootstrap manifest does not declare synthetic fixture modules"),
                ManifestContent.Contains(TEXT("synthetic_scene")));
            TestFalse(
                TEXT("STATUS-026 not expanded: bootstrap graph free of synthetic fixture"),
                ManifestContent.Contains(TEXT("synthetic_commands")));
        }
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
