#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/WrapBox.h"

// DCA-15 (ADR-0035, LayoutInvariant M4): a two-half gate over the claim
// "a value that defines layout is derived from the actually allotted space,
// or is an explicit, reasoned exception" -- the class of problem the
// WrapSize=1200 finding (Docs/Status/GV2_CommandPanelWrapSizeFixedNonScaling_2026-09-03.md)
// turned out to belong to, not a one-off. Neither half is a file list: both
// are computed by scanning/walking, so a future violation of either shape is
// caught without a test edit, the same guarantee DCA-08's Cast-gate and the
// PCC-10 capability sweep already give their own surface.

namespace
{
// --- Half A: Source/ scan -------------------------------------------------
//
// Grammar: the eight function names ADR-0035's own audit named
// (Docs/Plans/DeclaredCompositeAdoption/LayoutInvariant.md, DCA-15) supply an
// absolute layout dimension. A call counts as viewport-derived if its own
// argument text shows the evidence for it -- a call to EvaluateTextScale or
// GetViewportHeight, the two functions every current fix in this codebase
// routes through (UGV2UiTheme::EvaluateTextScale, UGV2TextPipeline::GetViewportHeight).
// Relocating a literal into a named C++ constant does not launder it: a bare
// constant reference (Theme->SomeMaxSize with no scale applied) shows no such
// evidence either and is flagged exactly like a raw literal -- the shortcut
// LayoutInvariant.md's own DCA-15 entry rules out ("вынесение 1200 в
// FGV2LayoutConstants -- это меняет место хранения, а не природу значения").
//
// SetPadding is scored by a narrower rule: it is common and expected
// throughout this codebase for inter-element padding to come from an
// unscaled Theme property (e.g. RichTextPopoverPadding, DropdownPopupPadding)
// -- that is not the class of bug DCA-15 closes. Only a *literal* digit
// written directly into a SetPadding call site is flagged, matching
// LayoutInvariant.md's own "SetPadding с ненулевыми литералами" wording.
const TArray<FString>& GetSizeFunctionNames()
{
    static const TArray<FString> Names = {
        TEXT("SetWidthOverride"),
        TEXT("SetHeightOverride"),
        TEXT("SetMinDesiredWidth"),
        TEXT("SetMinDesiredHeight"),
        TEXT("SetMaxDesiredWidth"),
        TEXT("SetMaxDesiredHeight"),
        TEXT("SetSize"),
        TEXT("SetWrapSize"),
    };
    return Names;
}

struct FGV2LayoutSourceMatch
{
    FString FileName;
    int32 LineNumber = 0;
    FString FunctionName;
    FString ArgumentWindow;
    // Text immediately preceding the call, wide enough to hold a local
    // "const float Scale = ...EvaluateTextScale(...)" one or two lines
    // above -- the common, more readable shape a real fix takes instead of
    // inlining the whole scale expression into the call itself. The
    // viewport-derivation check searches this text too, not only the call's
    // own argument list.
    FString PrecedingContext;
    bool bIsPaddingRule = false;
};

struct FGV2LayoutSourceException
{
    FString FileNameSubstring;
    FString FunctionName;
    FString Reason;
};

const TArray<FGV2LayoutSourceException>& GetLayoutSourceExceptions()
{
    static const TArray<FGV2LayoutSourceException> Exceptions = {
        {
            TEXT("GV2SeparatorWidgetBase.cpp"),
            TEXT("SetHeightOverride"),
            TEXT("Hairline divider thickness: a viewport-scaled fractional-pixel line would render blurrier at every resolution rather than crisper at any of them; the separator is one screen-space pixel by design, not a layout dimension ADR-0035 means to distribute."),
        },
        {
            TEXT("GV2SeparatorWidgetBase.cpp"),
            TEXT("SetWidthOverride"),
            TEXT("Hairline divider thickness: same reasoning as SetHeightOverride above (vertical-orientation separators use the width axis instead)."),
        },
    };
    return Exceptions;
}

bool IsWordBoundary(TCHAR Ch)
{
    return !FChar::IsAlnum(Ch) && Ch != '_';
}

bool ContainsWholeIdentifier(const FString& Text, const FString& Identifier)
{
    int32 SearchFrom = 0;
    for (;;)
    {
        const int32 Found = Text.Find(Identifier, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom);
        if (Found == INDEX_NONE)
        {
            return false;
        }
        const bool bLeftOk = Found == 0 || IsWordBoundary(Text[Found - 1]);
        const int32 AfterEnd = Found + Identifier.Len();
        const bool bRightOk = AfterEnd >= Text.Len() || IsWordBoundary(Text[AfterEnd]);
        if (bLeftOk && bRightOk)
        {
            return true;
        }
        SearchFrom = Found + Identifier.Len();
    }
}

// Names of local variables assigned from an expression that itself calls
// EvaluateTextScale/GetViewportHeight, found anywhere in PrecedingContext
// (e.g. "const float ViewportScale = Theme->EvaluateTextScale(...);" a line
// or two above the call this is checked for). Deliberately does not just
// check "does the marker text appear anywhere nearby": that would make an
// unrelated call in the same function look derived merely by proximity to a
// real one -- exactly the false negative a synthetic literal planted next to
// a genuine fix must not produce.
TArray<FString> FindScaleVariableNames(const FString& PrecedingContext)
{
    TArray<FString> Names;
    for (const TCHAR* Marker : {TEXT("EvaluateTextScale"), TEXT("GetViewportHeight")})
    {
        int32 SearchFrom = 0;
        for (;;)
        {
            const int32 MarkerPos = PrecedingContext.Find(Marker, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom);
            if (MarkerPos == INDEX_NONE)
            {
                break;
            }
            SearchFrom = MarkerPos + FCString::Strlen(Marker);

            const int32 Equals = PrecedingContext.Find(TEXT("="), ESearchCase::CaseSensitive, ESearchDir::FromEnd, MarkerPos);
            if (Equals == INDEX_NONE || Equals >= MarkerPos)
            {
                continue;
            }
            int32 NameEnd = Equals;
            while (NameEnd > 0 && FChar::IsWhitespace(PrecedingContext[NameEnd - 1]))
            {
                --NameEnd;
            }
            int32 NameStart = NameEnd;
            while (NameStart > 0 && (FChar::IsAlnum(PrecedingContext[NameStart - 1]) || PrecedingContext[NameStart - 1] == TEXT('_')))
            {
                --NameStart;
            }
            if (NameStart < NameEnd)
            {
                Names.AddUnique(PrecedingContext.Mid(NameStart, NameEnd - NameStart));
            }
        }
    }
    return Names;
}

bool ArgumentShowsViewportDerivation(const FGV2LayoutSourceMatch& Match)
{
    if (Match.ArgumentWindow.Contains(TEXT("EvaluateTextScale")) || Match.ArgumentWindow.Contains(TEXT("GetViewportHeight")))
    {
        return true;
    }
    for (const FString& ScaleVariable : FindScaleVariableNames(Match.PrecedingContext))
    {
        if (ContainsWholeIdentifier(Match.ArgumentWindow, ScaleVariable))
        {
            return true;
        }
    }
    return false;
}

bool ArgumentIsEffectivelyZero(const FString& ArgumentWindow)
{
    // True only when the argument is built entirely from zero literals and
    // punctuation (e.g. "0.0f", "FMargin(0.0f, 0.0f, 0.0f, 0.0f)") -- no
    // identifier characters (a bare constant/property reference) and no
    // nonzero digit. A property reference with zero literal digits at all
    // (e.g. "Theme->SeparatorThickness") must NOT read as "zero": it is a
    // real, unscaled value with no numeral in it to inspect.
    for (int32 Index = 0; Index < ArgumentWindow.Len(); ++Index)
    {
        const TCHAR Ch = ArgumentWindow[Index];
        if (FChar::IsDigit(Ch))
        {
            if (Ch != TEXT('0'))
            {
                return false;
            }
            continue;
        }
        if (Ch == TEXT('f') || Ch == TEXT('F') || Ch == TEXT('.') || Ch == TEXT(',')
            || Ch == TEXT('(') || Ch == TEXT(')') || FChar::IsWhitespace(Ch))
        {
            continue;
        }
        // Any other character (a letter that isn't the float suffix, '-',
        // '*', '>', etc.) means this argument carries real content -- an
        // identifier, an expression, or a nonzero-looking construct -- and
        // is not "just zero".
        return false;
    }
    return true;
}

// DCA-15: SetPadding's own, narrower rule -- only a raw numeric literal
// token written directly into the call counts (LayoutInvariant.md's own
// "SetPadding с ненулевыми литералами"). A bare property/constant reference
// (Theme->RichTextPopoverPadding, ContentPadding) is left alone here even
// though it carries no viewport-derivation marker: unscaled inter-element
// padding from a Theme property is the ordinary, accepted shape throughout
// this codebase and is not the class of bug this task closes -- only
// RecoveryScreenWidget's raw "20.0f" literal was. A digit is "raw" (not part
// of an identifier like a hypothetical PaddingV2 property name) when the
// character before it is not itself an identifier character.
bool ArgumentHasRawNonzeroNumericLiteral(const FString& ArgumentWindow)
{
    for (int32 Index = 0; Index < ArgumentWindow.Len(); ++Index)
    {
        if (!FChar::IsDigit(ArgumentWindow[Index]))
        {
            continue;
        }
        const bool bPartOfIdentifier = Index > 0
            && (FChar::IsAlpha(ArgumentWindow[Index - 1]) || ArgumentWindow[Index - 1] == TEXT('_'));
        if (bPartOfIdentifier)
        {
            continue;
        }
        int32 End = Index;
        bool bSawNonZeroDigit = ArgumentWindow[End] != TEXT('0');
        ++End;
        while (End < ArgumentWindow.Len() && (FChar::IsDigit(ArgumentWindow[End]) || ArgumentWindow[End] == TEXT('.')))
        {
            if (FChar::IsDigit(ArgumentWindow[End]) && ArgumentWindow[End] != TEXT('0'))
            {
                bSawNonZeroDigit = true;
            }
            ++End;
        }
        if (bSawNonZeroDigit)
        {
            return true;
        }
        Index = End - 1;
    }
    return false;
}

// One statement's worth of argument text: from the character right after the
// function name's opening '(' up to that statement's terminating ';', capped
// at a generous window so a malformed/unbalanced scan can never run away.
FString CaptureArgumentWindow(const FString& FileContent, int32 OpenParenPos)
{
    const int32 MaxWindow = 400;
    const int32 End = FMath::Min(FileContent.Len(), OpenParenPos + MaxWindow);
    FString Window = FileContent.Mid(OpenParenPos + 1, End - OpenParenPos - 1);
    int32 Semicolon = INDEX_NONE;
    if (Window.FindChar(TEXT(';'), Semicolon))
    {
        Window = Window.Left(Semicolon);
    }
    return Window;
}

// Enough of the preceding text to hold a locally-computed scale factor a few
// statements above the call itself (see FGV2LayoutSourceMatch::PrecedingContext).
FString CapturePrecedingContext(const FString& FileContent, int32 CallNamePos)
{
    const int32 MaxWindow = 400;
    const int32 Start = FMath::Max(0, CallNamePos - MaxWindow);
    return FileContent.Mid(Start, CallNamePos - Start);
}

void ScanFileForLayoutSourceMatches(const FString& FileName, const FString& FileContent, TArray<FGV2LayoutSourceMatch>& OutMatches)
{
    auto FindAllOccurrences = [&FileContent](const FString& FuncName, TArray<int32>& OutPositions)
    {
        int32 SearchFrom = 0;
        for (;;)
        {
            const int32 Found = FileContent.Find(FuncName, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom);
            if (Found == INDEX_NONE)
            {
                break;
            }
            const bool bLeftBoundary = Found == 0 || IsWordBoundary(FileContent[Found - 1]);
            const int32 AfterName = Found + FuncName.Len();
            const bool bRightBoundaryIsCall = AfterName < FileContent.Len() && FileContent[AfterName] == TEXT('(');
            if (bLeftBoundary && bRightBoundaryIsCall)
            {
                OutPositions.Add(Found);
            }
            SearchFrom = Found + FuncName.Len();
        }
    };

    auto LineNumberAt = [&FileContent](int32 Pos) -> int32
    {
        int32 Line = 1;
        for (int32 Index = 0; Index < Pos && Index < FileContent.Len(); ++Index)
        {
            if (FileContent[Index] == TEXT('\n'))
            {
                ++Line;
            }
        }
        return Line;
    };

    for (const FString& FuncName : GetSizeFunctionNames())
    {
        TArray<int32> Positions;
        FindAllOccurrences(FuncName, Positions);
        for (const int32 Pos : Positions)
        {
            const int32 OpenParen = Pos + FuncName.Len();
            FGV2LayoutSourceMatch Match;
            Match.FileName = FileName;
            Match.LineNumber = LineNumberAt(Pos);
            Match.FunctionName = FuncName;
            Match.ArgumentWindow = CaptureArgumentWindow(FileContent, OpenParen);
            Match.PrecedingContext = CapturePrecedingContext(FileContent, Pos);
            Match.bIsPaddingRule = false;
            OutMatches.Add(Match);
        }
    }

    {
        TArray<int32> Positions;
        FindAllOccurrences(TEXT("SetPadding"), Positions);
        for (const int32 Pos : Positions)
        {
            const int32 OpenParen = Pos + FString(TEXT("SetPadding")).Len();
            FGV2LayoutSourceMatch Match;
            Match.FileName = FileName;
            Match.LineNumber = LineNumberAt(Pos);
            Match.FunctionName = TEXT("SetPadding");
            Match.ArgumentWindow = CaptureArgumentWindow(FileContent, OpenParen);
            Match.bIsPaddingRule = true;
            OutMatches.Add(Match);
        }
    }
}

bool IsExcepted(const FGV2LayoutSourceMatch& Match, FString& OutReason)
{
    for (const FGV2LayoutSourceException& Exception : GetLayoutSourceExceptions())
    {
        if (Match.FileName.Contains(Exception.FileNameSubstring) && Match.FunctionName == Exception.FunctionName)
        {
            OutReason = Exception.Reason;
            return true;
        }
    }
    return false;
}

// --- Half B: Content/ widget-tree walk ------------------------------------
//
// Same shape of rule, applied to what an asset bakes in directly rather than
// what C++ applies to it at runtime: a WrapBox whose wrap threshold is a
// Designer-authored constant (UseExplicitWrapSize=true) rather than the
// panel's own allotted width, or a SizeBox whose Width/Height/MinDesired/
// MaxDesired override is Designer-set to an absolute value. Reading straight
// off a freshly CreateWidget()'d instance's WidgetTree, before TakeWidget()
// ever runs NativePreConstruct, is what makes this the asset's own baked
// state rather than whatever a widget's C++ later computes and applies at
// runtime -- exactly the "почти чисты" distinction LayoutInvariant.md's own
// DCA-15 entry draws for WBP_Separator/WBP_DropdownSelect/WBP_RichTextPopover.
struct FGV2LayoutContentException
{
    FString AssetNameSubstring;
    FString Reason;
};

const TArray<FGV2LayoutContentException>& GetLayoutContentExceptions()
{
    static const TArray<FGV2LayoutContentException> Exceptions;
    return Exceptions;
}

bool IsContentExcepted(const FString& AssetName, FString& OutReason)
{
    for (const FGV2LayoutContentException& Exception : GetLayoutContentExceptions())
    {
        if (AssetName.Contains(Exception.AssetNameSubstring))
        {
            OutReason = Exception.Reason;
            return true;
        }
    }
    return false;
}

void WalkWidgetTreeForAbsoluteLayout(
    UWidgetTree* Tree,
    const FString& AssetName,
    TArray<FString>& OutViolations,
    int32& OutCheckedWidgetCount)
{
    if (Tree == nullptr)
    {
        return;
    }
    Tree->ForEachWidget([&](UWidget* Widget)
    {
        if (Widget == nullptr)
        {
            return;
        }
        ++OutCheckedWidgetCount;

        if (const UWrapBox* AsWrapBox = Cast<UWrapBox>(Widget))
        {
            if (AsWrapBox->UseExplicitWrapSize())
            {
                OutViolations.Add(FString::Printf(
                    TEXT("[%s] WrapBox '%s' has UseExplicitWrapSize=true (wrap threshold is a constant, not the panel's allotted width)"),
                    *AssetName, *Widget->GetName()));
            }
        }
        else if (const USizeBox* AsSizeBox = Cast<USizeBox>(Widget))
        {
            if (AsSizeBox->IsWidthOverride())
            {
                OutViolations.Add(FString::Printf(TEXT("[%s] SizeBox '%s' has an authored WidthOverride"), *AssetName, *Widget->GetName()));
            }
            if (AsSizeBox->IsHeightOverride())
            {
                OutViolations.Add(FString::Printf(TEXT("[%s] SizeBox '%s' has an authored HeightOverride"), *AssetName, *Widget->GetName()));
            }
            if (AsSizeBox->IsMinDesiredWidthOverride())
            {
                OutViolations.Add(FString::Printf(TEXT("[%s] SizeBox '%s' has an authored MinDesiredWidth"), *AssetName, *Widget->GetName()));
            }
            if (AsSizeBox->IsMinDesiredHeightOverride())
            {
                OutViolations.Add(FString::Printf(TEXT("[%s] SizeBox '%s' has an authored MinDesiredHeight"), *AssetName, *Widget->GetName()));
            }
            if (AsSizeBox->IsMaxDesiredWidthOverride())
            {
                OutViolations.Add(FString::Printf(TEXT("[%s] SizeBox '%s' has an authored MaxDesiredWidth"), *AssetName, *Widget->GetName()));
            }
            if (AsSizeBox->IsMaxDesiredHeightOverride())
            {
                OutViolations.Add(FString::Printf(TEXT("[%s] SizeBox '%s' has an authored MaxDesiredHeight"), *AssetName, *Widget->GetName()));
            }
        }
    });
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LayoutParameterViewportDerivationTest,
    "GV2.Runtime.UIKit.LayoutParameterViewportDerivation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LayoutParameterViewportDerivationTest::RunTest(const FString& Parameters)
{
    // ---- Half A: Source/GV2 scan ----
    const FString SourceRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/GV2"));
    TArray<FString> SourceFiles;
    IFileManager::Get().FindFilesRecursive(SourceFiles, *SourceRoot, TEXT("*.cpp"), true, false, false);

    TArray<FGV2LayoutSourceMatch> AllMatches;
    for (const FString& FilePath : SourceFiles)
    {
        FString FileContent;
        if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
        {
            continue;
        }
        ScanFileForLayoutSourceMatches(FPaths::GetCleanFilename(FilePath), FileContent, AllMatches);
    }

    TestTrue(TEXT("DCA-15: Half A scan found at least one call to the named size-setting functions"), AllMatches.Num() > 0);

    int32 SourceExceptedCount = 0;
    int32 SourceViolationCount = 0;
    for (const FGV2LayoutSourceMatch& Match : AllMatches)
    {
        const bool bViolates = Match.bIsPaddingRule
            ? ArgumentHasRawNonzeroNumericLiteral(Match.ArgumentWindow)
            : (!ArgumentShowsViewportDerivation(Match) && !ArgumentIsEffectivelyZero(Match.ArgumentWindow));

        if (!bViolates)
        {
            continue;
        }

        FString ExceptionReason;
        if (IsExcepted(Match, ExceptionReason))
        {
            ++SourceExceptedCount;
            continue;
        }

        ++SourceViolationCount;
        AddError(FString::Printf(
            TEXT("DCA-15 Half A: %s:%d calls %s with a value that is not derived from viewport and is not an explicit exception (argument: %s)"),
            *Match.FileName, Match.LineNumber, *Match.FunctionName, *Match.ArgumentWindow.TrimStartAndEnd().Left(80)));
    }
    TestEqual(
        *FString::Printf(TEXT("DCA-15 Half A: no unexplained absolute-size call sites in Source/GV2 (%d matches scanned, %d excepted)"), AllMatches.Num(), SourceExceptedCount),
        SourceViolationCount,
        0);

    // ---- Half B: Content/ widget-tree walk ----
    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    FARFilter UiAssetFilter;
    UiAssetFilter.PackagePaths.Add(TEXT("/Game/UI"));
    UiAssetFilter.PackagePaths.Add(TEXT("/Game/TextSystem/UI"));
    UiAssetFilter.PackagePaths.Add(TEXT("/Game/RH/UI"));
    UiAssetFilter.bRecursivePaths = true;
    TArray<FAssetData> UiAssets;
    AssetRegistryModule.Get().GetAssets(UiAssetFilter, UiAssets);

    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    TestNotNull(TEXT("DCA-15 Half B: standalone GameInstance is created"), GameInstance);
    if (GameInstance == nullptr)
    {
        return false;
    }
    GameInstance->AddToRoot();
    GameInstance->InitializeStandalone();
    UWorld* const TestWorld = GameInstance->GetWorld();

    int32 AssetsWalked = 0;
    int32 WidgetsChecked = 0;
    int32 ContentExceptedCount = 0;
    TArray<FString> ContentViolations;

    if (TestWorld != nullptr)
    {
        for (const FAssetData& Asset : UiAssets)
        {
            const FString AssetName = Asset.AssetName.ToString();
            if (!AssetName.StartsWith(TEXT("WBP_")))
            {
                continue;
            }
            const FString GeneratedClassPath = FString::Printf(TEXT("%s.%s_C"), *Asset.PackageName.ToString(), *AssetName);
            UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, *GeneratedClassPath);
            // Abstract screen/composite bases (e.g. WBP_ScreenBase) exist to be
            // subclassed, not instantiated -- CreateWidget correctly refuses them;
            // skip before attempting it instead of letting the engine log its own
            // "Abstract, Deprecated or Replaced classes" error into this test.
            if (WidgetClass == nullptr || WidgetClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
            {
                continue;
            }
            UUserWidget* Instance = CreateWidget<UUserWidget>(TestWorld, WidgetClass);
            if (Instance == nullptr || Instance->WidgetTree == nullptr)
            {
                continue;
            }
            ++AssetsWalked;

            TArray<FString> AssetViolations;
            WalkWidgetTreeForAbsoluteLayout(Instance->WidgetTree, AssetName, AssetViolations, WidgetsChecked);

            FString ExceptionReason;
            if (!AssetViolations.IsEmpty() && IsContentExcepted(AssetName, ExceptionReason))
            {
                ContentExceptedCount += AssetViolations.Num();
                continue;
            }
            ContentViolations.Append(AssetViolations);
        }
    }

    TestTrue(TEXT("DCA-15 Half B: at least one production WBP_* asset was walked"), AssetsWalked > 0);
    TestTrue(TEXT("DCA-15 Half B: at least one WrapBox/SizeBox widget was checked across all walked assets"), WidgetsChecked > 0);

    for (const FString& Violation : ContentViolations)
    {
        AddError(FString::Printf(TEXT("DCA-15 Half B: %s -- not derived from allotted space and not an explicit exception"), *Violation));
    }
    TestEqual(
        *FString::Printf(TEXT("DCA-15 Half B: no unexplained authored absolute layout properties across %d walked assets (%d excepted)"), AssetsWalked, ContentExceptedCount),
        ContentViolations.Num(),
        0);

    GameInstance->Shutdown();
    GameInstance->RemoveFromRoot();

    return true;
}

#endif
