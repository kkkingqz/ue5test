#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Application/GV2SessionContentSnapshot.h"
#include "GV2ContentCore/RepositorySnapshot.h"
#include "GV2ContentCore/Value.h"
#include "GV2ContentCore/CanonicalHash.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"
#include "Curves/CurveFloat.h"
#include "Curves/RichCurve.h"
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

    AddInfo(FString::Printf(
        TEXT("Reflection enumerated %d UGV2UiTheme properties for mutation coverage."),
        ThemeProperties.Num()));

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

    TestEqual(
        TEXT("Every reflection-enumerated UGV2UiTheme property was tested"),
        TestedPropertiesCount,
        ThemeProperties.Num());

    // CFC-AF-28: the effective core fallback participates in rendering even though it
    // is not a field of the configured authoring asset. Exercise the real candidate
    // builder twice with the same authoring theme and different fallback content.
    UGV2UiTheme* CoreFallback = UGV2UiTheme::GetCoreMinimalTheme();
    TestNotNull(TEXT("Core minimal fallback theme is available"), CoreFallback);
    if (CoreFallback == nullptr)
    {
        return false;
    }

    struct FCoreFallbackRestorer
    {
        UGV2UiTheme* Theme = nullptr;
        TMap<FString, FText> TextCatalog;
        TMap<FString, FText> FallbackTextCatalog;
        ~FCoreFallbackRestorer()
        {
            if (Theme != nullptr)
            {
                Theme->TextCatalog = MoveTemp(TextCatalog);
                Theme->FallbackTextCatalog = MoveTemp(FallbackTextCatalog);
            }
        }
    } CoreFallbackRestorer{CoreFallback, CoreFallback->TextCatalog, CoreFallback->FallbackTextCatalog};

    const FString FallbackProbeId = TEXT("core:text.test.effective_fallback_identity");
    CoreFallback->TextCatalog.Add(FallbackProbeId, FText::FromString(TEXT("Fallback A")));
    FGV2SessionContentSnapshot FallbackSnapshotA;
    FString FallbackErrorA;
    TestTrue(
        TEXT("Fallback snapshot A builds through the production candidate path"),
        BuildCandidateSnapshot(BaseTheme.Get(), FallbackSnapshotA, FallbackErrorA));

    FGV2SessionContentSnapshot FallbackSnapshotARepeat;
    FString FallbackErrorARepeat;
    TestTrue(
        TEXT("Repeated fallback snapshot A builds through the production candidate path"),
        BuildCandidateSnapshot(BaseTheme.Get(), FallbackSnapshotARepeat, FallbackErrorARepeat));
    TestEqual(
        TEXT("Unchanged effective fallback has deterministic resolved Theme identity"),
        FString(UTF8_TO_TCHAR(GV2ContentCore::ComputeCanonicalHash(FallbackSnapshotARepeat.GetTheme().GetCanonicalValue()).c_str())),
        FString(UTF8_TO_TCHAR(GV2ContentCore::ComputeCanonicalHash(FallbackSnapshotA.GetTheme().GetCanonicalValue()).c_str())));

    auto ComputeScreenIdentityHash = [](const FGV2SessionContentSnapshot& Snapshot)
    {
        std::vector<GV2ContentCore::FValue> Entries;
        for (const TPair<FString, FString>& Identity : Snapshot.GetScreenRegistry().GetResolvedScreenIdentities())
        {
            Entries.push_back(GV2ContentCore::FValue::MakeObject({
                {"screen_id", GV2ContentCore::FValue::MakeString(TCHAR_TO_UTF8(*Identity.Key))},
                {"widget_class", GV2ContentCore::FValue::MakeString(TCHAR_TO_UTF8(*Identity.Value))},
            }));
        }
        return GV2ContentCore::ComputeCanonicalHash(GV2ContentCore::FValue::MakeArray(std::move(Entries)));
    };
    auto ComputeResourceIdentityHash = [](const FGV2SessionContentSnapshot& Snapshot)
    {
        TArray<FGV2ImageResourceDefinition> ResourceEntries = Snapshot.GetImageCatalog().Catalog->GetEntries();
        ResourceEntries.Sort([](const FGV2ImageResourceDefinition& A, const FGV2ImageResourceDefinition& B)
        {
            return A.ResourceId < B.ResourceId;
        });
        std::vector<GV2ContentCore::FValue> Entries;
        for (const FGV2ImageResourceDefinition& Entry : ResourceEntries)
        {
            Entries.push_back(GV2ContentCore::FValue::MakeObject({
                {"resource_id", GV2ContentCore::FValue::MakeString(TCHAR_TO_UTF8(*Entry.ResourceId))},
                {"pixel_hash", GV2ContentCore::FValue::MakeString(TCHAR_TO_UTF8(*Entry.CanonicalPixelHash))},
                {"render_mode", GV2ContentCore::FValue::MakeInteger(static_cast<std::int64_t>(Entry.RenderMode))},
                {"fixed_aspect_ratio", GV2ContentCore::FValue::MakeNumber(Entry.FixedAspectRatio)},
                {"nine_slice_left", GV2ContentCore::FValue::MakeNumber(Entry.NineSliceBorderPixels.Left)},
                {"nine_slice_top", GV2ContentCore::FValue::MakeNumber(Entry.NineSliceBorderPixels.Top)},
                {"nine_slice_right", GV2ContentCore::FValue::MakeNumber(Entry.NineSliceBorderPixels.Right)},
                {"nine_slice_bottom", GV2ContentCore::FValue::MakeNumber(Entry.NineSliceBorderPixels.Bottom)},
                {"tile_size_x", GV2ContentCore::FValue::MakeNumber(Entry.TileSize.X)},
                {"tile_size_y", GV2ContentCore::FValue::MakeNumber(Entry.TileSize.Y)},
            }));
        }
        return GV2ContentCore::ComputeCanonicalHash(GV2ContentCore::FValue::MakeArray(std::move(Entries)));
    };
    TestEqual(
        TEXT("Unchanged candidate has deterministic Screen Registry identity"),
        FString(UTF8_TO_TCHAR(ComputeScreenIdentityHash(FallbackSnapshotARepeat).c_str())),
        FString(UTF8_TO_TCHAR(ComputeScreenIdentityHash(FallbackSnapshotA).c_str())));
    TestEqual(
        TEXT("Unchanged candidate has deterministic Image Catalog identity"),
        FString(UTF8_TO_TCHAR(ComputeResourceIdentityHash(FallbackSnapshotARepeat).c_str())),
        FString(UTF8_TO_TCHAR(ComputeResourceIdentityHash(FallbackSnapshotA).c_str())));
    TestEqual(
        TEXT("Unchanged candidate has deterministic GameShell identity"),
        GetPathNameSafe(FallbackSnapshotARepeat.GetGameShellClass()),
        GetPathNameSafe(FallbackSnapshotA.GetGameShellClass()));
    TestEqual(
        TEXT("Unchanged effective fallback has deterministic PresentationHash"),
        FallbackSnapshotARepeat.GetPresentationHash(),
        FallbackSnapshotA.GetPresentationHash());
    TestEqual(
        TEXT("Unchanged effective fallback has deterministic SessionContentId"),
        FallbackSnapshotARepeat.GetSessionContentId(),
        FallbackSnapshotA.GetSessionContentId());

    CoreFallback->TextCatalog.Add(FallbackProbeId, FText::FromString(TEXT("Fallback B")));
    FGV2SessionContentSnapshot FallbackSnapshotB;
    FString FallbackErrorB;
    TestTrue(
        TEXT("Fallback snapshot B builds through the production candidate path"),
        BuildCandidateSnapshot(BaseTheme.Get(), FallbackSnapshotB, FallbackErrorB));

    const FText* FallbackTextA = FallbackSnapshotA.GetTheme().FindText(FallbackProbeId);
    const FText* FallbackTextB = FallbackSnapshotB.GetTheme().FindText(FallbackProbeId);
    TestNotNull(TEXT("Snapshot A resolves its captured effective fallback"), FallbackTextA);
    TestNotNull(TEXT("Snapshot B resolves its captured effective fallback"), FallbackTextB);
    if (FallbackTextA != nullptr && FallbackTextB != nullptr)
    {
        TestEqual(TEXT("Snapshot A retains fallback A"), FallbackTextA->ToString(), TEXT("Fallback A"));
        TestEqual(TEXT("Snapshot B observes fallback B"), FallbackTextB->ToString(), TEXT("Fallback B"));
    }
    TestNotEqual(
        TEXT("Different effective fallback content changes PresentationHash"),
        FallbackSnapshotA.GetPresentationHash(),
        FallbackSnapshotB.GetPresentationHash());
    TestNotEqual(
        TEXT("Different effective fallback content changes SessionContentId"),
        FallbackSnapshotA.GetSessionContentId(),
        FallbackSnapshotB.GetSessionContentId());

    TStrongObjectPtr<UGV2UiTheme> FallbackThemeA(NewObject<UGV2UiTheme>(GetTransientPackage()));
    TStrongObjectPtr<UGV2UiTheme> FallbackThemeB(DuplicateObject<UGV2UiTheme>(FallbackThemeA.Get(), GetTransientPackage()));
    FallbackThemeA->TextCatalog.Add(FallbackProbeId, FText::FromString(TEXT("Fallback A")));
    FallbackThemeB->TextCatalog.Add(FallbackProbeId, FText::FromString(TEXT("Fallback B")));
    FGV2ResolvedUiTheme DirectResolvedA;
    FGV2ResolvedUiTheme DirectResolvedB;
    FString DirectErrorA;
    FString DirectErrorB;
    TestTrue(
        TEXT("Direct resolved fallback A compiles"),
        FGV2ResolvedUiTheme::Compile(BaseTheme.Get(), FallbackThemeA.Get(), DirectResolvedA, DirectErrorA));
    TestTrue(
        TEXT("Direct resolved fallback B compiles"),
        FGV2ResolvedUiTheme::Compile(BaseTheme.Get(), FallbackThemeB.Get(), DirectResolvedB, DirectErrorB));
    TestNotEqual(
        TEXT("Resolved canonical value distinguishes effective fallback content"),
        FString(UTF8_TO_TCHAR(GV2ContentCore::ComputeCanonicalHash(DirectResolvedA.GetCanonicalValue()).c_str())),
        FString(UTF8_TO_TCHAR(GV2ContentCore::ComputeCanonicalHash(DirectResolvedB.GetCanonicalValue()).c_str())));
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

    // Assign an external curve asset to TextScaleCurve
    UCurveFloat* ExternalCurve = NewObject<UCurveFloat>(GetTransientPackage());
    ExternalCurve->FloatCurve.AddKey(1080.0f, 2.5f);
    AuthoredTheme->TextScaleCurve.ExternalCurve = ExternalCurve;

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
    TestNull(TEXT("Snapshot A severed ExternalCurve pointer to guarantee immutability"),
        SnapshotA.GetTheme().TextScaleCurve.ExternalCurve.Get());
    TestEqual(TEXT("Snapshot A evaluates text scale from snapshotted curve data"),
        SnapshotA.GetTheme().EvaluateTextScale(1080.0f), 2.5f);
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
    // Mutate the external curve asset in-place
    ExternalCurve->FloatCurve.Reset();
    ExternalCurve->FloatCurve.AddKey(1080.0f, 5.0f);

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
    TestEqual(TEXT("Snapshot A text scale remains 2.5f despite ExternalCurve mutation"),
        SnapshotA.GetTheme().EvaluateTextScale(1080.0f), 2.5f);
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
        SnapshotA.GetTheme().EvaluateTextScale(1080.0f), 2.5f);

    // 5. Build Snapshot B from the mutated theme asset
    FGV2SessionContentSnapshot SnapshotB;
    FString ErrorB;
    TestTrue(TEXT("Snapshot B builds successfully"), BuildCandidateSnapshot(AuthoredTheme.Get(), SnapshotB, ErrorB));

    // Snapshot B observes new values
    TestEqual(TEXT("Snapshot B sees mutated separator thickness 42.0f"),
        SnapshotB.GetTheme().SeparatorThickness, 42.0f);
    TestEqual(TEXT("Snapshot B sees mutated default text token"),
        SnapshotB.GetTheme().DefaultTextStyleToken, FName(TEXT("mutated_after_publication")));
    TestEqual(TEXT("Snapshot B observes mutated external curve text scale 5.0f"),
        SnapshotB.GetTheme().EvaluateTextScale(1080.0f), 5.0f);
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

// SAC-04: Deterministic canonical hashing test on UGV2UiTheme.
// Verifies that two UGV2UiTheme instances with identical keys and values in maps
// inserted in different/reverse orders produce identical canonical Value and presentation hash.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiThemeMapInsertionOrderTest,
    "GV2.Runtime.Presentation.UiThemeMapInsertionOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiThemeMapInsertionOrderTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UGV2UiTheme> Theme1(NewObject<UGV2UiTheme>(GetTransientPackage()));
    TStrongObjectPtr<UGV2UiTheme> Theme2(NewObject<UGV2UiTheme>(GetTransientPackage()));

    // Insert keys in order A, B, C, D into Theme1
    Theme1->TextCatalog.Add(TEXT("alpha"), FText::FromString(TEXT("ValAlpha")));
    Theme1->TextCatalog.Add(TEXT("beta"), FText::FromString(TEXT("ValBeta")));
    Theme1->TextCatalog.Add(TEXT("gamma"), FText::FromString(TEXT("ValGamma")));
    Theme1->TextCatalog.Add(TEXT("delta"), FText::FromString(TEXT("ValDelta")));

    Theme1->FallbackTextCatalog.Add(TEXT("fallback.first"), FText::FromString(TEXT("FB1")));
    Theme1->FallbackTextCatalog.Add(TEXT("fallback.second"), FText::FromString(TEXT("FB2")));

    Theme1->TextStyleTokens.FindOrAdd(TEXT("token_a"));
    Theme1->TextStyleTokens.FindOrAdd(TEXT("token_b"));
    Theme1->TextStyleTokens.FindOrAdd(TEXT("token_c"));

    Theme1->TextColorTokens.Add(TEXT("red"), FLinearColor::Red);
    Theme1->TextColorTokens.Add(TEXT("green"), FLinearColor::Green);
    Theme1->TextColorTokens.Add(TEXT("blue"), FLinearColor::Blue);

    Theme1->TextSizeTokens.Add(TEXT("small"), 12.0f);
    Theme1->TextSizeTokens.Add(TEXT("medium"), 16.0f);
    Theme1->TextSizeTokens.Add(TEXT("large"), 24.0f);

    // Insert keys in reverse order into Theme2
    Theme2->TextCatalog.Add(TEXT("delta"), FText::FromString(TEXT("ValDelta")));
    Theme2->TextCatalog.Add(TEXT("gamma"), FText::FromString(TEXT("ValGamma")));
    Theme2->TextCatalog.Add(TEXT("beta"), FText::FromString(TEXT("ValBeta")));
    Theme2->TextCatalog.Add(TEXT("alpha"), FText::FromString(TEXT("ValAlpha")));

    Theme2->FallbackTextCatalog.Add(TEXT("fallback.second"), FText::FromString(TEXT("FB2")));
    Theme2->FallbackTextCatalog.Add(TEXT("fallback.first"), FText::FromString(TEXT("FB1")));

    Theme2->TextStyleTokens.FindOrAdd(TEXT("token_c"));
    Theme2->TextStyleTokens.FindOrAdd(TEXT("token_b"));
    Theme2->TextStyleTokens.FindOrAdd(TEXT("token_a"));

    Theme2->TextColorTokens.Add(TEXT("blue"), FLinearColor::Blue);
    Theme2->TextColorTokens.Add(TEXT("green"), FLinearColor::Green);
    Theme2->TextColorTokens.Add(TEXT("red"), FLinearColor::Red);

    Theme2->TextSizeTokens.Add(TEXT("large"), 24.0f);
    Theme2->TextSizeTokens.Add(TEXT("medium"), 16.0f);
    Theme2->TextSizeTokens.Add(TEXT("small"), 12.0f);

    const GV2ContentCore::FValue Val1 = FGV2ResolvedUiTheme::ComputeThemeCanonicalValue(Theme1.Get());
    const GV2ContentCore::FValue Val2 = FGV2ResolvedUiTheme::ComputeThemeCanonicalValue(Theme2.Get());

    const std::string Hash1 = GV2ContentCore::ComputeCanonicalHash(Val1);
    const std::string Hash2 = GV2ContentCore::ComputeCanonicalHash(Val2);

    TestEqual(TEXT("Canonical hash is invariant under map insertion order"),
        FString(UTF8_TO_TCHAR(Hash1.c_str())), FString(UTF8_TO_TCHAR(Hash2.c_str())));

    FGV2ResolvedUiTheme Resolved1;
    FGV2ResolvedUiTheme Resolved2;
    FString Error1;
    FString Error2;
    TestTrue(TEXT("Resolved1 compiled successfully"), FGV2ResolvedUiTheme::Compile(Theme1.Get(), nullptr, Resolved1, Error1));
    TestTrue(TEXT("Resolved2 compiled successfully"), FGV2ResolvedUiTheme::Compile(Theme2.Get(), nullptr, Resolved2, Error2));

    const std::string ResolvedHash1 = GV2ContentCore::ComputeCanonicalHash(Resolved1.GetCanonicalValue());
    const std::string ResolvedHash2 = GV2ContentCore::ComputeCanonicalHash(Resolved2.GetCanonicalValue());

    TestEqual(TEXT("Resolved theme canonical hash is invariant under map insertion order"),
        FString(UTF8_TO_TCHAR(ResolvedHash1.c_str())), FString(UTF8_TO_TCHAR(ResolvedHash2.c_str())));

    return true;
}

#endif
