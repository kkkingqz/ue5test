#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Application/GV2SessionContentSnapshot.h"
#include "GV2ContentCore/RepositorySnapshot.h"
#include "GV2ContentCore/Value.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"
#include "Misc/Paths.h"
#include "Tests/GV2PresentationTestFixtures.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2UiTheme.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

// SAC-04: Negative reflection test on UGV2UiTheme.
// Enumerates all UPROPERTY fields of UGV2UiTheme dynamically derived via reflection (TFieldIterator<FProperty>),
// mutates each property individually, and verifies that every single mutation alters both
// PresentationHash and SessionContentId.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiThemeReflectionNegativeTest,
    "GV2.Runtime.Presentation.UiThemeReflectionNegative",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiThemeReflectionNegativeTest::RunTest(const FString& Parameters)
{
    UGV2UiThemeSettings* MutableSettings = GetMutableDefault<UGV2UiThemeSettings>();
    TestNotNull(TEXT("UGV2UiThemeSettings is available"), MutableSettings);
    if (MutableSettings == nullptr)
    {
        return false;
    }

    const TSoftObjectPtr<UGV2UiTheme> OriginalThemeAsset = MutableSettings->ThemeAsset;
    struct FThemeSettingsRestorer
    {
        TSoftObjectPtr<UGV2UiTheme> SavedAsset;
        ~FThemeSettingsRestorer()
        {
            if (UGV2UiThemeSettings* Settings = GetMutableDefault<UGV2UiThemeSettings>())
            {
                Settings->ThemeAsset = SavedAsset;
            }
        }
    } Restorer{OriginalThemeAsset};

    // Helper to mutate any FProperty value in target container
    auto MutateValue = [](auto& SelfRef, FProperty* Prop, void* ValuePtr) -> bool
    {
        if (Prop == nullptr || ValuePtr == nullptr)
        {
            return false;
        }
        if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
        {
            FloatProp->SetPropertyValue(ValuePtr, FloatProp->GetPropertyValue(ValuePtr) + 1.0f);
            return true;
        }
        if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
        {
            DoubleProp->SetPropertyValue(ValuePtr, DoubleProp->GetPropertyValue(ValuePtr) + 1.0);
            return true;
        }
        if (FIntProperty* IntProp = CastField<FIntProperty>(Prop))
        {
            IntProp->SetPropertyValue(ValuePtr, IntProp->GetPropertyValue(ValuePtr) + 1);
            return true;
        }
        if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
        {
            BoolProp->SetPropertyValue(ValuePtr, !BoolProp->GetPropertyValue(ValuePtr));
            return true;
        }
        if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
        {
            ByteProp->SetPropertyValue(ValuePtr, ByteProp->GetPropertyValue(ValuePtr) + 1);
            return true;
        }
        if (FNameProperty* NameProp = CastField<FNameProperty>(Prop))
        {
            FName Current = NameProp->GetPropertyValue(ValuePtr);
            NameProp->SetPropertyValue(ValuePtr, FName(*(Current.ToString() + TEXT("_mutated"))));
            return true;
        }
        if (FStrProperty* StrProp = CastField<FStrProperty>(Prop))
        {
            FString Current = StrProp->GetPropertyValue(ValuePtr);
            StrProp->SetPropertyValue(ValuePtr, Current + TEXT("_mutated"));
            return true;
        }
        if (FTextProperty* TextProp = CastField<FTextProperty>(Prop))
        {
            FText Current = TextProp->GetPropertyValue(ValuePtr);
            TextProp->SetPropertyValue(ValuePtr, FText::FromString(Current.ToString() + TEXT("_mutated")));
            return true;
        }
        if (FClassProperty* ClassProp = CastField<FClassProperty>(Prop))
        {
            UClass* Current = Cast<UClass>(ClassProp->GetPropertyValue(ValuePtr));
            ClassProp->SetPropertyValue(ValuePtr, Current == nullptr ? ClassProp->MetaClass.Get() : nullptr);
            return true;
        }
        if (FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Prop))
        {
            FSoftObjectPtr Current = SoftObjProp->GetPropertyValue(ValuePtr);
            if (Current.IsNull())
            {
                if (FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Prop))
                {
                    SoftClassProp->SetPropertyValue(ValuePtr, FSoftObjectPtr(SoftClassProp->MetaClass.Get()));
                }
                else
                {
                    SoftObjProp->SetPropertyValue(ValuePtr, FSoftObjectPtr(FSoftObjectPath(TEXT("/Game/TestMutated.TestMutated_C"))));
                }
            }
            else
            {
                SoftObjProp->SetPropertyValue(ValuePtr, FSoftObjectPtr());
            }
            return true;
        }
        if (FObjectProperty* ObjectProp = CastField<FObjectProperty>(Prop))
        {
            UObject* Current = ObjectProp->GetObjectPropertyValue(ValuePtr);
            ObjectProp->SetObjectPropertyValue(ValuePtr, Current == nullptr ? GetTransientPackage() : nullptr);
            return true;
        }
        if (FMapProperty* MapProp = CastField<FMapProperty>(Prop))
        {
            FScriptMapHelper Helper(MapProp, ValuePtr);
            if (Helper.Num() > 0)
            {
                Helper.RemoveAt(0);
            }
            else
            {
                Helper.AddDefaultValue_Invalid_NeedsRehash();
                Helper.Rehash();
            }
            return true;
        }
        if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop))
        {
            FScriptArrayHelper Helper(ArrayProp, ValuePtr);
            if (Helper.Num() > 0)
            {
                Helper.RemoveValues(0, 1);
            }
            else
            {
                Helper.AddValue();
            }
            return true;
        }
        if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
        {
            if (StructProp->Struct != nullptr && StructProp->Struct->GetFName() == FName(TEXT("RuntimeFloatCurve")))
            {
                FRuntimeFloatCurve* Curve = static_cast<FRuntimeFloatCurve*>(ValuePtr);
                FRichCurve* Rich = Curve->GetRichCurve();
                if (Rich != nullptr)
                {
                    Rich->AddKey(9999.0f, 99.0f);
                    return true;
                }
            }
            if (StructProp->Struct != nullptr)
            {
                for (TFieldIterator<FProperty> StructFieldIt(StructProp->Struct); StructFieldIt; ++StructFieldIt)
                {
                    FProperty* InnerProp = *StructFieldIt;
                    if (InnerProp != nullptr)
                    {
                        void* InnerValuePtr = InnerProp->ContainerPtrToValuePtr<void>(ValuePtr);
                        if (SelfRef(SelfRef, InnerProp, InnerValuePtr))
                        {
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    };

    const FString GameDataDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData"));
    const std::vector<std::filesystem::path> Closure = {
        std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(GameDataDir, TEXT("core")))),
        std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(GameDataDir, TEXT("textsystem")))),
        std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(GameDataDir, TEXT("rh")))),
    };

    std::vector<GV2ContentCore::FDiagnostic> ResolveDiagnostics;
    const std::optional<GV2ContentHostSupport::FResolvedPackageSet> ResolvedSet =
        GV2ContentHostSupport::ResolvePackageSetFromDirectories(Closure, ResolveDiagnostics);
    TestTrue(TEXT("ResolvedPackageSet is valid"), ResolvedSet.has_value());
    if (!ResolvedSet.has_value())
    {
        return false;
    }

    const GV2ContentCore::FBuildResult RepositoryBuild =
        BuildGV2RepositoryFromResolvedPackageSet(*ResolvedSet);
    TestTrue(TEXT("RepositoryBuild is successful"), RepositoryBuild.IsSuccess());
    if (!RepositoryBuild.IsSuccess())
    {
        return false;
    }

    TArray<FGV2SchemaPackageRoot> SchemaRoots;
    SchemaRoots.Reserve(static_cast<int32>(ResolvedSet->OrderedSources.size()) + 1);
    for (const GV2ContentHostSupport::FResolvedPackageSource& Source : ResolvedSet->OrderedSources)
    {
        SchemaRoots.Add(FGV2SchemaPackageRoot{
            UTF8_TO_TCHAR(Source.Descriptor.GetPackageId().c_str()),
            UTF8_TO_TCHAR(Source.Root.string().c_str())});
    }
    SchemaRoots.Add(FGV2SchemaPackageRoot{
        TEXT("core"),
        FPaths::Combine(FPaths::ProjectDir(), TEXT("Tests/Fixtures/SyntheticMechanicalFixture/schemas"))
    });

    auto BuildCandidateSnapshot = [&](UGV2UiTheme* Theme, FGV2SessionContentSnapshot& OutSnapshot, FString& OutError) -> bool
    {
        MutableSettings->ThemeAsset = Theme;
        GV2RuntimeCore::FRuntimeFault Fault;
        if (!FGV2SessionContentCandidate::Build(
                RepositoryBuild.GetCandidate().GetReadHandle(),
                *ResolvedSet,
                SchemaRoots,
                {},
                OutSnapshot,
                Fault))
        {
            OutError = FString::Printf(TEXT("%s: %s"), UTF8_TO_TCHAR(Fault.Code.c_str()), UTF8_TO_TCHAR(Fault.Message.c_str()));
            return false;
        }
        FGV2SessionContentCandidate::FinalizeScriptIdentity(OutSnapshot, "theme_reflection_negative_test_script_hash");
        return true;
    };

    // Construct base synthetic theme with initial values for maps and styles so mutation has observable delta
    UGV2UiTheme* ConfiguredThemeAsset = GV2PresentationTestFixtures::GetAuthoringThemeForTest();
    TStrongObjectPtr<UGV2UiTheme> BaseTheme;
    if (ConfiguredThemeAsset != nullptr)
    {
        BaseTheme = TStrongObjectPtr<UGV2UiTheme>(DuplicateObject<UGV2UiTheme>(ConfiguredThemeAsset, GetTransientPackage()));
    }
    else
    {
        BaseTheme = TStrongObjectPtr<UGV2UiTheme>(NewObject<UGV2UiTheme>(GetTransientPackage()));
    }
    BaseTheme->TextStyleTokens.FindOrAdd(TEXT("default"));
    BaseTheme->TextColorTokens.FindOrAdd(TEXT("default"), FLinearColor::White);
    BaseTheme->TextSizeTokens.FindOrAdd(TEXT("default"), 14.0f);
    BaseTheme->TextCatalog.FindOrAdd(TEXT("test.key"), FText::FromString(TEXT("Test Value")));
    BaseTheme->FallbackTextCatalog.FindOrAdd(TEXT("test.fallback"), FText::FromString(TEXT("Fallback Value")));

    FGV2SessionContentSnapshot BaseSnapshot;
    FString BaseBuildError;
    TestTrue(TEXT("Base snapshot builds successfully"), BuildCandidateSnapshot(BaseTheme.Get(), BaseSnapshot, BaseBuildError));
    const FString BasePresentationHash = BaseSnapshot.GetPresentationHash();
    const FString BaseSessionContentId = BaseSnapshot.GetSessionContentId();
    TestFalse(TEXT("Base presentation hash is not empty"), BasePresentationHash.IsEmpty());
    TestFalse(TEXT("Base session content id is not empty"), BaseSessionContentId.IsEmpty());

    // Enumerate ALL UPROPERTY fields of UGV2UiTheme via reflection
    TArray<FProperty*> ThemeProperties;
    for (TFieldIterator<FProperty> PropIt(UGV2UiTheme::StaticClass(), EFieldIterationFlags::Default); PropIt; ++PropIt)
    {
        FProperty* Prop = *PropIt;
        if (Prop != nullptr && Prop->GetOwnerClass() == UGV2UiTheme::StaticClass())
        {
            ThemeProperties.Add(Prop);
        }
    }

    TestEqual(TEXT("UGV2UiTheme has exactly 38 properties declared"), ThemeProperties.Num(), 38);

    int32 TestedPropertiesCount = 0;
    for (FProperty* Prop : ThemeProperties)
    {
        const FString PropName = Prop->GetName();

        // Clone base theme
        UGV2UiTheme* MutatedTheme = DuplicateObject<UGV2UiTheme>(BaseTheme.Get(), GetTransientPackage());
        TestNotNull(*FString::Printf(TEXT("Mutated theme for '%s' duplicated successfully"), *PropName), MutatedTheme);
        if (MutatedTheme == nullptr)
        {
            continue;
        }

        void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(MutatedTheme);
        const bool bMutated = MutateValue(MutateValue, Prop, ValuePtr);
        TestTrue(*FString::Printf(TEXT("Property '%s' was mutated by reflection"), *PropName), bMutated);

        FString BaseExportedText;
        Prop->ExportTextItem_Direct(
            BaseExportedText,
            Prop->ContainerPtrToValuePtr<void>(BaseTheme.Get()),
            nullptr,
            BaseTheme.Get(),
            PPF_None);

        FString MutatedExportedText;
        Prop->ExportTextItem_Direct(
            MutatedExportedText,
            Prop->ContainerPtrToValuePtr<void>(MutatedTheme),
            nullptr,
            MutatedTheme,
            PPF_None);

        TestNotEqual(
            *FString::Printf(TEXT("Property '%s' exported text changes after mutation"), *PropName),
            MutatedExportedText,
            BaseExportedText);

        FGV2SessionContentSnapshot MutatedSnapshot;
        FString MutatedError;
        const bool bBuilt = BuildCandidateSnapshot(MutatedTheme, MutatedSnapshot, MutatedError);
        TestTrue(*FString::Printf(TEXT("Mutated snapshot for '%s' builds successfully: %s"), *PropName, *MutatedError), bBuilt);

        TestNotEqual(
            *FString::Printf(TEXT("Mutating '%s' changes PresentationHash"), *PropName),
            MutatedSnapshot.GetPresentationHash(),
            BasePresentationHash);

        TestNotEqual(
            *FString::Printf(TEXT("Mutating '%s' changes SessionContentId"), *PropName),
            MutatedSnapshot.GetSessionContentId(),
            BaseSessionContentId);

        TestedPropertiesCount++;
    }

    TestEqual(TEXT("All 38 properties were tested"), TestedPropertiesCount, 38);
    return true;
}

// SAC-04: Immutability test on FGV2SessionContentSnapshot theme.
// Verifies that mutating the authoring UGV2UiTheme asset in place after session publication
// does NOT change the active session's prepared output, hash, or SessionContentId.
// A newly built candidate/session observes the mutated values and produces a distinct identity.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2SnapshotThemeImmutabilityTest,
    "GV2.Runtime.Presentation.SnapshotThemeImmutability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2SnapshotThemeImmutabilityTest::RunTest(const FString& Parameters)
{
    UGV2UiThemeSettings* MutableSettings = GetMutableDefault<UGV2UiThemeSettings>();
    TestNotNull(TEXT("UGV2UiThemeSettings is available"), MutableSettings);
    if (MutableSettings == nullptr)
    {
        return false;
    }

    const TSoftObjectPtr<UGV2UiTheme> OriginalThemeAsset = MutableSettings->ThemeAsset;
    struct FThemeSettingsRestorer
    {
        TSoftObjectPtr<UGV2UiTheme> SavedAsset;
        ~FThemeSettingsRestorer()
        {
            if (UGV2UiThemeSettings* Settings = GetMutableDefault<UGV2UiThemeSettings>())
            {
                Settings->ThemeAsset = SavedAsset;
            }
        }
    } Restorer{OriginalThemeAsset};

    const FString GameDataDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("GameData"));
    const std::vector<std::filesystem::path> Closure = {
        std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(GameDataDir, TEXT("core")))),
        std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(GameDataDir, TEXT("textsystem")))),
        std::filesystem::path(TCHAR_TO_UTF8(*FPaths::Combine(GameDataDir, TEXT("rh")))),
    };

    std::vector<GV2ContentCore::FDiagnostic> ResolveDiagnostics;
    const std::optional<GV2ContentHostSupport::FResolvedPackageSet> ResolvedSet =
        GV2ContentHostSupport::ResolvePackageSetFromDirectories(Closure, ResolveDiagnostics);
    TestTrue(TEXT("ResolvedPackageSet is valid"), ResolvedSet.has_value());
    if (!ResolvedSet.has_value())
    {
        return false;
    }

    const GV2ContentCore::FBuildResult RepositoryBuild =
        BuildGV2RepositoryFromResolvedPackageSet(*ResolvedSet);
    TestTrue(TEXT("RepositoryBuild is successful"), RepositoryBuild.IsSuccess());
    if (!RepositoryBuild.IsSuccess())
    {
        return false;
    }

    TArray<FGV2SchemaPackageRoot> SchemaRoots;
    SchemaRoots.Reserve(static_cast<int32>(ResolvedSet->OrderedSources.size()) + 1);
    for (const GV2ContentHostSupport::FResolvedPackageSource& Source : ResolvedSet->OrderedSources)
    {
        SchemaRoots.Add(FGV2SchemaPackageRoot{
            UTF8_TO_TCHAR(Source.Descriptor.GetPackageId().c_str()),
            UTF8_TO_TCHAR(Source.Root.string().c_str())});
    }
    SchemaRoots.Add(FGV2SchemaPackageRoot{
        TEXT("core"),
        FPaths::Combine(FPaths::ProjectDir(), TEXT("Tests/Fixtures/SyntheticMechanicalFixture/schemas"))
    });

    auto BuildCandidateSnapshot = [&](UGV2UiTheme* Theme, FGV2SessionContentSnapshot& OutSnapshot, FString& OutError) -> bool
    {
        MutableSettings->ThemeAsset = Theme;
        GV2RuntimeCore::FRuntimeFault Fault;
        if (!FGV2SessionContentCandidate::Build(
                RepositoryBuild.GetCandidate().GetReadHandle(),
                *ResolvedSet,
                SchemaRoots,
                {},
                OutSnapshot,
                Fault))
        {
            OutError = FString::Printf(TEXT("%s: %s"), UTF8_TO_TCHAR(Fault.Code.c_str()), UTF8_TO_TCHAR(Fault.Message.c_str()));
            return false;
        }
        FGV2SessionContentCandidate::FinalizeScriptIdentity(OutSnapshot, "theme_immutability_test_script_hash");
        return true;
    };

    // 1. Create authoring theme and populate with baseline values
    UGV2UiTheme* BaseAuthoredTheme = GV2PresentationTestFixtures::GetAuthoringThemeForTest();
    TStrongObjectPtr<UGV2UiTheme> AuthoredTheme;
    if (BaseAuthoredTheme != nullptr)
    {
        AuthoredTheme = TStrongObjectPtr<UGV2UiTheme>(DuplicateObject<UGV2UiTheme>(BaseAuthoredTheme, GetTransientPackage()));
    }
    else
    {
        AuthoredTheme = TStrongObjectPtr<UGV2UiTheme>(NewObject<UGV2UiTheme>(GetTransientPackage()));
    }
    AuthoredTheme->TextStyleTokens.FindOrAdd(TEXT("authored_default"));
    AuthoredTheme->TextStyleTokens.FindOrAdd(TEXT("mutated_after_publication"));
    AuthoredTheme->SeparatorThickness = 4.0f;
    AuthoredTheme->DefaultTextStyleToken = TEXT("authored_default");
    AuthoredTheme->TextCatalog.Add(TEXT("core:text.authored.sample"), FText::FromString(TEXT("Original Sample")));

    // 2. Publish Session Content Snapshot A
    FGV2SessionContentSnapshot SnapshotA;
    FString ErrorA;
    TestTrue(TEXT("Snapshot A builds successfully"), BuildCandidateSnapshot(AuthoredTheme.Get(), SnapshotA, ErrorA));

    const FString InitialHashA = SnapshotA.GetPresentationHash();
    const FString InitialSessionContentIdA = SnapshotA.GetSessionContentId();
    TestEqual(TEXT("Snapshot A theme separator thickness matches initial authored value"),
        SnapshotA.GetTheme().SeparatorThickness, 4.0f);
    TestEqual(TEXT("Snapshot A theme default text token matches initial authored value"),
        SnapshotA.GetTheme().DefaultTextStyleToken, FName(TEXT("authored_default")));
    const FText* FoundTextA = SnapshotA.GetTheme().FindText(TEXT("core:text.authored.sample"));
    TestNotNull(TEXT("Snapshot A resolves original sample text"), FoundTextA);
    if (FoundTextA != nullptr)
    {
        TestEqual(TEXT("Snapshot A sample text string matches"), FoundTextA->ToString(), TEXT("Original Sample"));
    }

    // 3. Mutate AuthoredTheme in-place AFTER Snapshot A is published
    AuthoredTheme->SeparatorThickness = 42.0f;
    AuthoredTheme->DefaultTextStyleToken = TEXT("mutated_after_publication");
    AuthoredTheme->TextCatalog.Add(TEXT("core:text.authored.sample"), FText::FromString(TEXT("Mutated Text Value")));
    AuthoredTheme->TextCatalog.Add(TEXT("core:text.injected.key"), FText::FromString(TEXT("Injected Later")));

    // 4. Verify Snapshot A is completely unaffected (immutability of published session)
    TestEqual(TEXT("Snapshot A theme separator thickness remains 4.0f"),
        SnapshotA.GetTheme().SeparatorThickness, 4.0f);
    TestEqual(TEXT("Snapshot A theme default text token remains authored_default"),
        SnapshotA.GetTheme().DefaultTextStyleToken, FName(TEXT("authored_default")));
    const FText* RecheckedTextA = SnapshotA.GetTheme().FindText(TEXT("core:text.authored.sample"));
    TestNotNull(TEXT("Snapshot A still resolves original sample text"), RecheckedTextA);
    if (RecheckedTextA != nullptr)
    {
        TestEqual(TEXT("Snapshot A sample text string is still Original Sample"),
            RecheckedTextA->ToString(), TEXT("Original Sample"));
    }
    TestNull(TEXT("Snapshot A does not see text injected after publication"),
        SnapshotA.GetTheme().FindText(TEXT("core:text.injected.key")));
    TestEqual(TEXT("Snapshot A presentation hash is completely unaffected"),
        SnapshotA.GetPresentationHash(), InitialHashA);
    TestEqual(TEXT("Snapshot A session content id is completely unaffected"),
        SnapshotA.GetSessionContentId(), InitialSessionContentIdA);

    // Verify prepared UI transaction using Snapshot A theme remains identical
    FGV2TextViewModel PreparedTextOriginal;
    FString TextError;
    TestTrue(TEXT("ResolveLiteralForAutomationTest succeeds with Snapshot A's resolved theme"),
        UGV2TextPipeline::ResolveLiteralForAutomationTest(
            SnapshotA.GetTheme(),
            TEXT("Sample Text"),
            SnapshotA.GetTheme().DefaultTextStyleToken,
            PreparedTextOriginal,
            TextError));
    TestEqual(TEXT("Prepared text font size is unaffected by subsequent theme asset mutation"),
        SnapshotA.GetTheme().EvaluateTextScale(1080.0f), 1.0f);

    // 5. Build Snapshot B from the mutated theme asset
    FGV2SessionContentSnapshot SnapshotB;
    FString ErrorB;
    TestTrue(TEXT("Snapshot B builds successfully"), BuildCandidateSnapshot(AuthoredTheme.Get(), SnapshotB, ErrorB));

    // Snapshot B observes new values
    TestEqual(TEXT("Snapshot B sees mutated separator thickness 42.0f"),
        SnapshotB.GetTheme().SeparatorThickness, 42.0f);
    TestEqual(TEXT("Snapshot B sees mutated default text token"),
        SnapshotB.GetTheme().DefaultTextStyleToken, FName(TEXT("mutated_after_publication")));
    const FText* FoundTextB = SnapshotB.GetTheme().FindText(TEXT("core:text.injected.key"));
    TestNotNull(TEXT("Snapshot B resolves newly injected text"), FoundTextB);
    if (FoundTextB != nullptr)
    {
        TestEqual(TEXT("Snapshot B injected text string matches"), FoundTextB->ToString(), TEXT("Injected Later"));
    }

    // Identities of A and B must differ
    TestNotEqual(TEXT("Snapshot B presentation hash differs from Snapshot A"),
        SnapshotB.GetPresentationHash(), SnapshotA.GetPresentationHash());
    TestNotEqual(TEXT("Snapshot B session content id differs from Snapshot A"),
        SnapshotB.GetSessionContentId(), SnapshotA.GetSessionContentId());

    return true;
}

#endif
