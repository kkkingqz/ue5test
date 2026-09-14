#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "ImageUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "CommonRichTextBlock.h"
#include "CommonTextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

#include "Application/GV2ScreenFieldMaterializer.h"
#include "UI/GV2CentralStylePreparer.h"

#include "Runtime/GV2RuntimeSubsystem.h"
#include "UI/GV2ButtonListWidgetBase.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2CheckboxWidgetBase.h"
#include "UI/GV2DeclaredCompositeWidgetBase.h"
#include "UI/GV2DropdownSelectWidgetBase.h"
#include "UI/GV2GameShellWidgetBase.h"
#include "UI/GV2IconWidgetBase.h"
#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2ImageWidgetBase.h"
#include "UI/GV2InputFieldWidgetBase.h"
#include "UI/GV2LayoutConstants.h"
#include "UI/GV2ListViewWidgetBase.h"
#include "UI/GV2LoadingIndicatorWidgetBase.h"
#include "UI/GV2ModalWidgetBase.h"
#include "UI/GV2PanelWidgetBase.h"
#include "UI/GV2PortraitWidgetBase.h"
#include "UI/GV2ProgressBarWidgetBase.h"
#include "UI/GV2RecoveryScreenWidget.h"
#include "UI/GV2RichTextPopoverWidgetBase.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2ScreenRegistry.h"
#include "UI/GV2ScreenWidgetBase.h"
#include "UI/GV2ScrollAreaWidgetBase.h"
#include "UI/GV2SeparatorWidgetBase.h"
#include "UI/GV2TabContainerWidgetBase.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2TextPipelineHost.h"
#include "UI/GV2TextWidgetBase.h"
#include "UI/GV2UiTheme.h"
#include "UI/GV2UiStyleConsumer.h"
#include "UI/GV2UiCapability.h"
#include "GV2PresentationApply/PreparedPresentationTransaction.h"

#include "Tests/GV2PresentationTestFixtures.h"

namespace
{
using GV2PresentationTestFixtures::LoadConfiguredThemeForTest;
using GV2PresentationTestFixtures::MakeResolvedLiteralTextForTest;

GV2PresentationApply::FPreparedRichTextStyle MakePreparedRichTextStyleForTest(
    const UGV2UiTheme& Theme)
{
    GV2PresentationApply::FPreparedRichTextStyle Result;
    Result.DefaultTokenName = Theme.DefaultTextStyleToken.IsNone()
        ? FName(TEXT("default"))
        : Theme.DefaultTextStyleToken;
    Result.DefaultStyleClass = Theme.RichTextStyle;
    Result.DefaultToken.StyleClass = Theme.RichTextStyle;
    Result.DefaultToken.UnscaledFontSize = 0.0f;
    if (const UCommonTextStyle* Style = Theme.RichTextStyle != nullptr
            ? Cast<UCommonTextStyle>(Theme.RichTextStyle->GetDefaultObject())
            : nullptr)
    {
        Style->ToTextBlockStyle(Result.DefaultToken.BaseStyle);
        Result.DefaultToken.bResolved = true;
    }
    Result.ColorByToken = Theme.TextColorTokens;
    Result.UnscaledSizeByToken = Theme.TextSizeTokens;
    Result.ScalePolicy = UGV2TextPipeline::ResolveScalePolicyForTheme(&Theme, Result.DefaultTokenName);
    Result.InteractiveStyle = Theme.RichTextInteractiveStyle;
    Result.PopoverClass = Theme.RichTextPopoverClass.LoadSynchronous();
    Result.PopoverStyle.Background = Theme.RichTextPopoverBackground;
    Result.PopoverStyle.Padding = Theme.RichTextPopoverPadding;
    Result.PopoverStyle.MaxWidth = Theme.RichTextPopoverMaxWidth;
    Result.PopoverStyle.MaxHeight = Theme.RichTextPopoverMaxHeight;
    Result.PopoverStyle.ImageTint = Theme.ImageTint;
    Result.PopoverStyle.Scale.ScaleCurve = Theme.TextScaleCurve;
    Result.PopoverStyle.Scale.ReferenceViewportHeight = Theme.ReferenceViewportHeight;
    Result.bIsResolved = true;
    return Result;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiScalingModelAndConstantsContract,
    "GV2.Runtime.UIKit.ScalingModelAndConstants",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiScalingModelAndConstantsContract::RunTest(const FString& Parameters)
{
    // 1. UIF-06: Dual Resolution Constants
    TestEqual(
        TEXT("Raster authoring width is 3840 (4K)"),
        FGV2LayoutConstants::RasterAuthoringWidth,
        3840.0f);
    TestEqual(
        TEXT("Raster authoring height is 2160 (4K)"),
        FGV2LayoutConstants::RasterAuthoringHeight,
        2160.0f);
    TestEqual(
        TEXT("Virtual layout unit width is 1920 (1080p)"),
        FGV2LayoutConstants::VirtualLayoutWidth,
        1920.0f);
    TestEqual(
        TEXT("Virtual layout unit height is 1080 (1080p)"),
        FGV2LayoutConstants::VirtualLayoutHeight,
        1080.0f);
    TestEqual(
        TEXT("Raster to layout scale factor is 2.0"),
        FGV2LayoutConstants::RasterToLayoutScale,
        2.0f);
    TestEqual(
        TEXT("Minimum supported viewport width is 1280 (720p)"),
        FGV2LayoutConstants::MinSupportedViewportWidth,
        1280.0f);
    TestEqual(
        TEXT("Minimum supported viewport height is 720 (720p)"),
        FGV2LayoutConstants::MinSupportedViewportHeight,
        720.0f);

    // 2. UIF-08: Primitive Scale Policy & Resource Compatibility
    TestTrue(
        TEXT("FreeStretch policy is compatible with Tile render mode"),
        IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::FreeStretch, EGV2ImageRenderMode::Tile));
    TestFalse(
        TEXT("FreeStretch policy is incompatible with NineSlice render mode"),
        IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::FreeStretch, EGV2ImageRenderMode::NineSlice));
    TestFalse(
        TEXT("FreeStretch policy is incompatible with FixedAspect render mode"),
        IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::FreeStretch, EGV2ImageRenderMode::FixedAspect));

    TestTrue(
        TEXT("Tile policy is compatible with Tile render mode"),
        IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::Tile, EGV2ImageRenderMode::Tile));
    TestFalse(
        TEXT("Tile policy is incompatible with NineSlice render mode"),
        IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::Tile, EGV2ImageRenderMode::NineSlice));

    TestTrue(
        TEXT("NineSlice policy is compatible with NineSlice render mode"),
        IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::NineSlice, EGV2ImageRenderMode::NineSlice));
    TestFalse(
        TEXT("NineSlice policy is incompatible with Tile render mode"),
        IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::NineSlice, EGV2ImageRenderMode::Tile));

    TestTrue(
        TEXT("PreserveAspect policy is compatible with FixedAspect render mode"),
        IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::PreserveAspect, EGV2ImageRenderMode::FixedAspect));
    TestFalse(
        TEXT("PreserveAspect policy is incompatible with NineSlice render mode"),
        IsScalePolicyCompatible(EGV2PrimitiveScalePolicy::PreserveAspect, EGV2ImageRenderMode::NineSlice));

    // 3. UIF-09: Text Scale Curve & Minimum Readable Font Size
    UGV2UiTheme* Theme = NewObject<UGV2UiTheme>();
    TestNotNull(TEXT("Transient theme instance created"), Theme);
    if (Theme != nullptr)
    {
        Theme->TextSizeTokens.Add(TEXT("body"), 14.0f);
        Theme->TextSizeTokens.Add(TEXT("small"), 10.0f);
        Theme->TextSizeTokens.Add(TEXT("heading"), 22.0f);

        // Evaluation at standard heights
        const float Scale720 = Theme->EvaluateTextScale(720.0f);
        const float Scale1080 = Theme->EvaluateTextScale(1080.0f);
        const float Scale1440 = Theme->EvaluateTextScale(1440.0f);
        const float Scale2160 = Theme->EvaluateTextScale(2160.0f);

        TestTrue(TEXT("Scale at 720p preserves readability (around 0.85)"), Scale720 >= 0.80f && Scale720 <= 0.90f);
        TestEqual(TEXT("Scale at 1080p is baseline (1.0)"), Scale1080, 1.0f);
        TestTrue(TEXT("Scale at 1440p grows modestly (around 1.25)"), Scale1440 >= 1.20f && Scale1440 <= 1.30f);
        TestTrue(TEXT("Scale at 4K (2160p) is bounded (around 1.60)"), Scale2160 >= 1.50f && Scale2160 <= 1.70f);

        // Monotonic growth
        TestTrue(TEXT("Scale grows monotonically: 720p <= 1080p"), Scale720 <= Scale1080);
        TestTrue(TEXT("Scale grows monotonically: 1080p <= 1440p"), Scale1080 <= Scale1440);
        TestTrue(TEXT("Scale grows monotonically: 1440p <= 2160p"), Scale1440 <= Scale2160);

        // Minimum readable font size threshold (10 pt)
        const float SmallSizeAt720 = Theme->GetEffectiveFontSize(TEXT("small"), 720.0f);
        TestTrue(
            TEXT("Effective font size never drops below MinReadableFontSize"),
            SmallSizeAt720 >= Theme->MinReadableFontSize);
        TestEqual(TEXT("Small size at 720p clamped to MinReadableFontSize"), SmallSizeAt720, 10.0f);

        const float BodySizeAt720 = Theme->GetEffectiveFontSize(TEXT("body"), 720.0f);
        TestTrue(TEXT("Body text size at 720p is readable (>= 10pt)"), BodySizeAt720 >= 10.0f);
    }

    // 4. UIF-10: Resolution Matrix Coverage
    struct FResolutionTarget
    {
        float Width;
        float Height;
        const TCHAR* Label;
        bool bIsUltrawide;
    };

    const FResolutionTarget ResolutionMatrix[] = {
        { 3840.0f, 2160.0f, TEXT("4K UHD (16:9)"), false },
        { 2560.0f, 1440.0f, TEXT("QHD (16:9)"), false },
        { 1920.0f, 1080.0f, TEXT("FHD (16:9)"), false },
        { 1280.0f, 720.0f,  TEXT("HD (16:9 minimum target)"), false },
        { 3440.0f, 1440.0f, TEXT("UWQHD (21:9)"), true },
        { 2560.0f, 1080.0f, TEXT("UWFHD (21:9)"), true }
    };

    for (const FResolutionTarget& Target : ResolutionMatrix)
    {
        const float Aspect = Target.Width / Target.Height;
        if (Target.bIsUltrawide)
        {
            TestTrue(
                *FString::Printf(TEXT("%s aspect ratio is ultrawide (~2.33)"), Target.Label),
                FMath::IsNearlyEqual(Aspect, FGV2LayoutConstants::UltrawideAspectRatio, 0.06f));
        }
        else
        {
            TestTrue(
                *FString::Printf(TEXT("%s aspect ratio is standard 16:9 (~1.78)"), Target.Label),
                FMath::IsNearlyEqual(Aspect, FGV2LayoutConstants::StandardAspectRatio, 0.01f));
        }

        TestTrue(
            *FString::Printf(TEXT("%s width >= MinSupportedViewportWidth"), Target.Label),
            Target.Width >= FGV2LayoutConstants::MinSupportedViewportWidth);
        TestTrue(
            *FString::Printf(TEXT("%s height >= MinSupportedViewportHeight"), Target.Label),
            Target.Height >= FGV2LayoutConstants::MinSupportedViewportHeight);

        if (Theme != nullptr)
        {
            const float Scale = Theme->EvaluateTextScale(Target.Height);
            TestTrue(
                *FString::Printf(TEXT("%s evaluated scale is positive and bounded"), Target.Label),
                Scale >= 0.80f && Scale <= 2.0f);
            const float BodyFontSize = Theme->GetEffectiveFontSize(TEXT("body"), Target.Height);
            TestTrue(
                *FString::Printf(TEXT("%s body font size >= MinReadableFontSize"), Target.Label),
                BodyFontSize >= Theme->MinReadableFontSize);
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiCoreBaselineAdaptersContract,
    "GV2.Runtime.UIKit.CoreBaselineAdapters",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiCoreBaselineAdaptersContract::RunTest(const FString& Parameters)
{
    GV2PresentationTestFixtures::FPrepareContextFixture ContextFixture;
    FString ContextError;
    const bool bContextReady = ContextFixture.Initialize(ContextError);
    TestTrue(
        *FString::Printf(TEXT("Presentation Prepare context builds [Error: %s]"), *ContextError),
        bContextReady);
    const FGV2PresentationPrepareContext* PrepareContext = ContextFixture.Get();
    if (!bContextReady || PrepareContext == nullptr)
    {
        return false;
    }

    if (UGV2UiTheme* Theme = LoadConfiguredThemeForTest())
    {
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.progress.health"), FText::FromString(TEXT("Health")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.modal.title"), FText::FromString(TEXT("Title")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.modal.content"), FText::FromString(TEXT("Content")));
        Theme->FallbackTextCatalog.FindOrAdd(TEXT("core:text.button.ok"), FText::FromString(TEXT("OK")));
    }

    // UPP-16..30: the per-schema FAdapter mechanism, and then the registry/singleton
    // class wrapping it, were both deleted outright, not just emptied -- every
    // schema_id (baseline widgets and LocationScreen alike) now resolves through
    // the schema-driven GV2ScreenFieldMaterializer free functions.

    // 5. Generic Binding Extraction for Location Commands
    {
        GV2RuntimeCore::FScreenRequest ValidReq;
        ValidReq.ScreenId = "textsystem:screen.location";
        GV2RuntimeCore::FScreenField CmdField;
        CmdField.FieldId = "commands";
        CmdField.SchemaId = "textsystem:schema.ui_field.location_commands.v1";
        GV2RuntimeCore::FValue::FObject CmdObj;

        GV2RuntimeCore::FValue::FArray ItemsArray;
        GV2RuntimeCore::FValue::FObject Btn1;
        Btn1["key"] = GV2RuntimeCore::FValue(std::string("btn_talk"));
        GV2RuntimeCore::FValue::FObject TextObj;
        TextObj["text_id"] = GV2RuntimeCore::FValue(std::string("core:text.talk"));
        Btn1["text"] = GV2RuntimeCore::FValue(TextObj);
        Btn1["binding"] = GV2RuntimeCore::FValue(std::string("core:command.talk"));
        ItemsArray.push_back(GV2RuntimeCore::FValue(Btn1));

        CmdObj["items"] = GV2RuntimeCore::FValue(ItemsArray);
        CmdField.Value = GV2RuntimeCore::FValue(MoveTemp(CmdObj));
        ValidReq.Fields.push_back(MoveTemp(CmdField));

        TArray<FGV2UiBindingDefinition> Defs;
        TestTrue(TEXT("Generic binding extraction succeeds for commands"), GV2ScreenFieldMaterializer::PrepareBindingDefinitions(*PrepareContext, ValidReq, Defs));
        TestEqual(TEXT("Extracted 1 binding definition"), Defs.Num(), 1);
        if (Defs.Num() == 1)
        {
            TestEqual(TEXT("Binding element id matches"), Defs[0].ElementId, FString(TEXT("textsystem:screen.location#widget.btn_talk")));
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiCoreBaselineComponentsContract,
    "GV2.Runtime.UIKit.CoreBaselineComponents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiCoreBaselineComponentsContract::RunTest(const FString& Parameters)
{
    // 1. Panel component defaults
    {
        UGV2PanelWidgetBase* Panel = NewObject<UGV2PanelWidgetBase>();
        TestNotNull(TEXT("Transient panel widget created"), Panel);
        if (Panel != nullptr)
        {
            TestEqual(TEXT("Panel default scale policy is NineSlice"), Panel->GetScalePolicy(), EGV2PrimitiveScalePolicy::NineSlice);
            TestEqual(TEXT("Panel default content padding is 16"), Panel->GetContentPadding().Left, 16.0f);
            Panel->SetContentPadding(FMargin(24.0f));
            TestEqual(TEXT("Panel updated content padding is 24"), Panel->GetContentPadding().Left, 24.0f);
        }
    }

    // 2. ScrollArea component defaults
    {
        UGV2ScrollAreaWidgetBase* ScrollArea = NewObject<UGV2ScrollAreaWidgetBase>();
        TestNotNull(TEXT("Transient scroll area widget created"), ScrollArea);
        if (ScrollArea != nullptr)
        {
            TestEqual(TEXT("ScrollArea default orientation is vertical"), ScrollArea->GetOrientation(), EOrientation::Orient_Vertical);
            TestEqual(TEXT("ScrollArea initial scroll offset is 0"), ScrollArea->GetScrollOffset(), 0.0f);
        }
    }

    // 3. ListView component defaults
    {
        UGV2ListViewWidgetBase* ListView = NewObject<UGV2ListViewWidgetBase>();
        TestNotNull(TEXT("Transient list view widget created"), ListView);
        if (ListView != nullptr)
        {
            TestEqual(TEXT("ListView default orientation is vertical"), ListView->GetOrientation(), EOrientation::Orient_Vertical);
            TestEqual(TEXT("ListView initial entry count is 0"), ListView->GetEntryCount(), 0);
            ListView->SetOrientation(EOrientation::Orient_Horizontal);
            TestEqual(TEXT("ListView updated orientation is horizontal"), ListView->GetOrientation(), EOrientation::Orient_Horizontal);
        }
    }

    // 4. Icon component defaults
    {
        UGV2IconWidgetBase* Icon = NewObject<UGV2IconWidgetBase>();
        TestNotNull(TEXT("Transient icon widget created"), Icon);
        if (Icon != nullptr)
        {
            TestEqual(TEXT("Icon default scale policy is PreserveAspect"), Icon->GetScalePolicy(), EGV2PrimitiveScalePolicy::PreserveAspect);
        }
    }

    // 5. Dynamic Screen Element interface implementations
    {
        UGV2PortraitWidgetBase* Portrait = NewObject<UGV2PortraitWidgetBase>();
        TestNotNull(TEXT("Transient portrait widget created"), Portrait);
        if (Portrait != nullptr)
        {
            FGV2UiCapabilityBuilder Builder;
            Portrait->DescribeUiCapabilities(Builder);
            const FGV2UiCapabilityTree Caps = Builder.Build();
            TestTrue(TEXT("Portrait declares key capability"), Caps.Properties.Contains(TEXT("key")));
        }

        UGV2ModalWidgetBase* Modal = NewObject<UGV2ModalWidgetBase>();
        TestNotNull(TEXT("Transient modal widget created"), Modal);
        if (Modal != nullptr)
        {
            FGV2UiCapabilityBuilder Builder;
            Modal->DescribeUiCapabilities(Builder);
            const FGV2UiCapabilityTree Caps = Builder.Build();
            TestTrue(TEXT("Modal declares title capability"), Caps.Properties.Contains(TEXT("title")));
            TestTrue(TEXT("Modal declares content capability"), Caps.Properties.Contains(TEXT("content")));
            TestTrue(TEXT("Modal declares buttons capability"), Caps.Properties.Contains(TEXT("buttons")));
            TestTrue(TEXT("Modal declares backdrop_close_action capability"), Caps.Properties.Contains(TEXT("backdrop_close_action")));
            TestTrue(TEXT("Modal declares key capability"), Caps.Properties.Contains(TEXT("key")));
        }

        UGV2ProgressBarWidgetBase* ProgressBar = NewObject<UGV2ProgressBarWidgetBase>();
        TestNotNull(TEXT("Transient progress bar widget created"), ProgressBar);
        if (ProgressBar != nullptr)
        {
            FGV2UiCapabilityBuilder Builder;
            ProgressBar->DescribeUiCapabilities(Builder);
            const FGV2UiCapabilityTree Caps = Builder.Build();
            TestTrue(TEXT("ProgressBar declares percent capability"), Caps.Properties.Contains(TEXT("percent")));
        }

        UGV2ImageWidgetBase* Image = NewObject<UGV2ImageWidgetBase>();
        TestNotNull(TEXT("Transient image widget created"), Image);
        if (Image != nullptr)
        {
            FGV2UiCapabilityBuilder Builder;
            Image->DescribeUiCapabilities(Builder);
            const FGV2UiCapabilityTree Caps = Builder.Build();
            TestTrue(TEXT("Image declares resource_id capability"), Caps.Properties.Contains(TEXT("resource_id")));
        }

        UGV2RichTextWidgetBase* RichText = NewObject<UGV2RichTextWidgetBase>();
        TestNotNull(TEXT("Transient rich text widget created"), RichText);
        if (RichText != nullptr)
        {
            FGV2UiCapabilityBuilder Builder;
            RichText->DescribeUiCapabilities(Builder);
            const FGV2UiCapabilityTree Caps = Builder.Build();
            TestTrue(TEXT("RichText declares text capability"), Caps.Properties.Contains(TEXT("text")));
            TestTrue(TEXT("RichText declares spans capability"), Caps.Properties.Contains(TEXT("spans")));
            TestTrue(TEXT("RichText declares key capability"), Caps.Properties.Contains(TEXT("key")));
        }

        UGV2RichTextPopoverWidgetBase* Popover = NewObject<UGV2RichTextPopoverWidgetBase>();
        TestNotNull(TEXT("Transient popover widget created"), Popover);
        if (Popover != nullptr)
        {
            FGV2UiCapabilityBuilder Builder;
            Popover->DescribeUiCapabilities(Builder);
            const FGV2UiCapabilityTree Caps = Builder.Build();
            TestTrue(TEXT("Popover declares title capability"), Caps.Properties.Contains(TEXT("title")));
            TestTrue(TEXT("Popover declares description capability"), Caps.Properties.Contains(TEXT("description")));
            TestTrue(TEXT("Popover declares key capability"), Caps.Properties.Contains(TEXT("key")));
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiKitCentralThemeContract,
    "GV2.Runtime.UIKit.CentralThemeAndComponents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiKitCentralThemeContract::RunTest(const FString& Parameters)
{
    UGV2UiTheme* Theme = LoadConfiguredThemeForTest();
    TestNotNull(TEXT("Configured central UI theme is loadable"), Theme);
    if (Theme == nullptr)
    {
        return false;
    }

    TestNotNull(TEXT("Theme provides the default text style"), Theme->TextStyle.Get());
    TestNotNull(TEXT("Theme provides the rich text style"), Theme->RichTextStyle.Get());
    TestNotNull(
        TEXT("Theme provides the rich text popover class"),
        Theme->RichTextPopoverClass.LoadSynchronous());
    TestTrue(
        TEXT("Theme provides a visible rich text popover background"),
        Theme->RichTextPopoverBackground.DrawAs != ESlateBrushDrawType::NoDrawType);
    TestTrue(
        TEXT("Theme constrains rich text popover height for overflow scrolling"),
        Theme->RichTextPopoverMaxHeight >= 64.0f);
    TestNotNull(TEXT("Theme provides the button style"), Theme->ButtonStyle.Get());
    TestNotNull(TEXT("Theme provides the button label style"), Theme->ButtonLabelStyle.Get());
    TestNotNull(TEXT("Theme provides the checkbox label style"), Theme->CheckboxLabelStyle.Get());
    TestNotNull(TEXT("Theme provides the input field label style"), Theme->InputFieldLabelStyle.Get());
    TestNotNull(TEXT("Theme provides the dropdown header style"), Theme->DropdownHeaderStyle.Get());
    TestTrue(
        TEXT("Theme provides a visible dropdown popup background"),
        Theme->DropdownPopupBackground.DrawAs != ESlateBrushDrawType::NoDrawType);
    TestTrue(
        TEXT("Theme constrains dropdown popup height"),
        Theme->DropdownMaxPopupHeight >= 32.0f);
    TestTrue(
        TEXT("Theme provides a visible unchecked checkbox brush"),
        Theme->CheckboxStyle.UncheckedImage.DrawAs != ESlateBrushDrawType::NoDrawType);
    TestTrue(
        TEXT("Theme provides a visible checked checkbox brush"),
        Theme->CheckboxStyle.CheckedImage.DrawAs != ESlateBrushDrawType::NoDrawType);
    TestTrue(TEXT("Theme registers the default text token"), Theme->TextStyleTokens.Contains(TEXT("default")));
    TestTrue(TEXT("Theme registers the inventory text token"), Theme->TextStyleTokens.Contains(TEXT("inventory")));
    TestTrue(TEXT("Theme registers the blue color token"), Theme->TextColorTokens.Contains(TEXT("blue")));
    TestTrue(TEXT("Theme registers the huge size token"), Theme->TextSizeTokens.Contains(TEXT("huge")));

    // These fixtures exercise the pipeline's own mechanics (args, style token, markup
    // escaping below), not real game content, so they are registered here as synthetic
    // core-namespaced entries rather than depending on a catalog id owned by a non-core
    // game package (Source/GV2 cannot hardcode a dependency on such a namespace).
    Theme->TextCatalog.Add(TEXT("core:text.screen.test.description"), FText::FromString(TEXT("You are exploring with {player_name}.")));
    Theme->TextCatalog.Add(TEXT("core:text.screen.test.checkbox"), FText::FromString(TEXT("Enable feature")));
    Theme->TextCatalog.Add(TEXT("core:text.screen.test.name_label"), FText::FromString(TEXT("Name")));
    Theme->TextCatalog.Add(TEXT("core:text.screen.test.dropdown_placeholder"), FText::FromString(TEXT("Choose one...")));

    TestTrue(
        TEXT("Theme contains the test screen localized fixture"),
        Theme->TextCatalog.Contains(TEXT("core:text.screen.test.description")));
    TestTrue(
        TEXT("Theme contains the checkbox localized fixture"),
        Theme->TextCatalog.Contains(TEXT("core:text.screen.test.checkbox")));
    TestTrue(
        TEXT("Theme contains the input label localized fixture"),
        Theme->TextCatalog.Contains(TEXT("core:text.screen.test.name_label")));
    TestTrue(
        TEXT("Theme contains the dropdown localized fixture"),
        Theme->TextCatalog.Contains(TEXT("core:text.screen.test.dropdown_placeholder")));

    FString NormalizedMarkup;
    FString MarkupError;
    TestTrue(
        TEXT("Text pipeline accepts nested data-driven tokens"),
        UGV2TextPipeline::NormalizeMarkup(
            Theme,
            TEXT("A <color=blue>blue <size=huge>large</size></color><br/>line"),
            NormalizedMarkup,
            MarkupError));
    TestTrue(TEXT("Text pipeline flattens color runs"), NormalizedMarkup.Contains(TEXT("color=\"blue\"")));
    TestTrue(TEXT("Text pipeline flattens nested size runs"), NormalizedMarkup.Contains(TEXT("size=\"huge\"")));
    TestTrue(TEXT("Text pipeline converts semantic breaks"), NormalizedMarkup.Contains(TEXT("\nline")));
    TestFalse(
        TEXT("Text pipeline rejects unknown token values"),
        UGV2TextPipeline::NormalizeMarkup(
            Theme,
            TEXT("<color=not_registered>invalid</color>"),
            NormalizedMarkup,
            MarkupError));

    FGV2UiControlValue PlayerName;
    PlayerName.Name = TEXT("player_name");
    PlayerName.Type = EGV2UiControlValueType::String;
    PlayerName.StringValue = TEXT("<size=huge>Injected</size>");
    FGV2TextViewModel ResolvedText;
    FString ResolveError;
    TestTrue(
        TEXT("Text pipeline resolves text_id, arguments and optional style"),
        UGV2TextPipeline::ResolveForAutomationTest(
            Theme,
            TEXT("core:text.screen.test.description"),
            {PlayerName},
            TEXT("inventory"),
            ResolvedText,
            ResolveError));
    TestEqual(TEXT("Resolved text retains the semantic style token"), ResolvedText.StyleToken, FName(TEXT("inventory")));
    TestFalse(TEXT("Resolved text carries centrally prepared renderer markup"), ResolvedText.NormalizedMarkup.IsEmpty());
    TestTrue(TEXT("String arguments cannot inject markup"), ResolvedText.Text.ToString().Contains(TEXT("&lt;size=huge&gt;")));
    TestTrue(
        TEXT("Escaped arguments remain single-escaped during markup normalization"),
        UGV2TextPipeline::NormalizeMarkup(Theme, ResolvedText.Text.ToString(), NormalizedMarkup, MarkupError));
    TestEqual(TEXT("Resolved renderer markup is the canonical normalized output"), ResolvedText.NormalizedMarkup, NormalizedMarkup);
    TestTrue(TEXT("Escaped argument is preserved for the renderer"), NormalizedMarkup.Contains(TEXT("&lt;size=huge&gt;")));
    TestFalse(TEXT("Escaped argument is not double-escaped"), NormalizedMarkup.Contains(TEXT("&amp;lt;size=huge")));

    // LOC-07: Missing translation in TextCatalog smoothly falls back to FallbackTextCatalog (source_message)
    Theme->FallbackTextCatalog.Add(TEXT("core:text.untranslated.item"), FText::FromString(TEXT("Fallback source string")));
    FGV2TextViewModel FallbackResolvedText;
    FString FallbackResolveError;
    TestTrue(
        TEXT("Text pipeline falls back to FallbackTextCatalog when key is missing from TextCatalog"),
        UGV2TextPipeline::ResolveForAutomationTest(
            Theme,
            TEXT("core:text.untranslated.item"),
            {},
            TEXT("inventory"),
            FallbackResolvedText,
            FallbackResolveError));
    TestEqual(TEXT("Fallback resolved text matches source_message"), FallbackResolvedText.Text.ToString(), TEXT("Fallback source string"));

    FGV2TextViewModel MissingResolvedText;
    FString MissingResolveError;
    TestFalse(
        TEXT("Text pipeline rejects completely unknown text_id without fault or crash"),
        UGV2TextPipeline::ResolveForAutomationTest(
            Theme,
            TEXT("core:text.unknown.nonexistent"),
            {},
            TEXT("inventory"),
            MissingResolvedText,
            MissingResolveError));
    TestTrue(TEXT("Error message identifies unknown text_id"), MissingResolveError.Contains(TEXT("Unknown text_id")));

    TestNull(
        TEXT("Plain text base exposes no raw FText apply entry point"),
        UGV2TextWidgetBase::StaticClass()->FindFunctionByName(TEXT("ApplyTextContent")));
    TestNull(
        TEXT("Rich text base exposes no raw FText apply entry point"),
        UGV2RichTextWidgetBase::StaticClass()->FindFunctionByName(TEXT("ApplyRichTextContent")));
    TestNull(
        TEXT("Image base exposes no raw Slate brush mutation entry point"),
        UGV2ImageWidgetBase::StaticClass()->FindFunctionByName(TEXT("ApplyImageBrush")));

    FString DerivedResourceId;
    FString ImagePathError;
    const FString ResourceRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("Resources"));
    TestTrue(
        TEXT("Image resource_id is derived from the canonical relative path"),
        UGV2ImageResourceCatalog::TryMakeResourceId(
            ResourceRoot,
            FPaths::Combine(
                ResourceRoot,
                TEXT("core/resource/image/character_portrait.png")),
            DerivedResourceId,
            ImagePathError));
    TestEqual(
        TEXT("Recursive image path maps to the expected Stable ID"),
        DerivedResourceId,
        FString(TEXT("core:resource.image.character_portrait")));
    TestTrue(
        TEXT("Tile suffix is accepted as source metadata"),
        UGV2ImageResourceCatalog::TryMakeResourceId(
            ResourceRoot,
            FPaths::Combine(
                ResourceRoot,
                TEXT("core/resource/ui/old_paper_tile_256.tile.png")),
            DerivedResourceId,
            ImagePathError));
    TestEqual(
        TEXT("Tile suffix is omitted from the Stable ID"),
        DerivedResourceId,
        FString(TEXT("core:resource.ui.old_paper_tile_256")));
    TestTrue(
        TEXT("Nine-slice suffix is accepted as source metadata"),
        UGV2ImageResourceCatalog::TryMakeResourceId(
            ResourceRoot,
            FPaths::Combine(ResourceRoot, TEXT("core/resource/ui/panel.9.png")),
            DerivedResourceId,
            ImagePathError));
    TestEqual(
        TEXT("Nine-slice suffix is omitted from the Stable ID"),
        DerivedResourceId,
        FString(TEXT("core:resource.ui.panel")));
    TestFalse(
        TEXT("Non-canonical image path is rejected"),
        UGV2ImageResourceCatalog::TryMakeResourceId(
            ResourceRoot,
            FPaths::Combine(ResourceRoot, TEXT("core/resource/Image/Portrait.png")),
            DerivedResourceId,
            ImagePathError));

    const FString ScannerFixtureRoot = FPaths::Combine(
        FPaths::ProjectIntermediateDir(),
        TEXT("GV2AutomationImageResources"));
    const FString ScannerFixtureDirectory = FPaths::Combine(
        ScannerFixtureRoot,
        TEXT("core/resource/image"));
    IFileManager::Get().DeleteDirectory(*ScannerFixtureRoot, false, true);
    IFileManager::Get().MakeDirectory(*ScannerFixtureDirectory, true);
    FImage ScannerFixtureImage(4, 6, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
    FMemory::Memset(ScannerFixtureImage.RawData.GetData(), 255, ScannerFixtureImage.RawData.Num());
    const FString ScannerFixturePng = FPaths::Combine(
        ScannerFixtureDirectory,
        TEXT("character_portrait.png"));
    TestTrue(
        TEXT("Scanner fixture PNG is written"),
        FImageUtils::SaveImageByExtension(*ScannerFixturePng, ScannerFixtureImage));

    FImage NineSliceFixtureImage(6, 6, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
    FMemory::Memzero(
        NineSliceFixtureImage.RawData.GetData(),
        NineSliceFixtureImage.RawData.Num());
    FColor* NineSlicePixels = reinterpret_cast<FColor*>(NineSliceFixtureImage.RawData.GetData());
    for (int32 Y = 1; Y < 5; ++Y)
    {
        for (int32 X = 1; X < 5; ++X)
        {
            NineSlicePixels[Y * 6 + X] = FColor::White;
        }
    }
    NineSlicePixels[2] = FColor::Black;
    NineSlicePixels[3] = FColor::Black;
    NineSlicePixels[2 * 6] = FColor::Black;
    NineSlicePixels[3 * 6] = FColor::Black;
    const FString NineSliceFixturePng = FPaths::Combine(
        ScannerFixtureDirectory,
        TEXT("panel.9.png"));
    TestTrue(
        TEXT("Nine-slice fixture PNG is written"),
        FImageUtils::SaveImageByExtension(*NineSliceFixturePng, NineSliceFixtureImage));

    UGV2ImageResourceCatalog* ScannedCatalog = NewObject<UGV2ImageResourceCatalog>();
    TestTrue(
        TEXT("Image catalog recursively scans and decodes filesystem PNG"),
        ScannedCatalog->BuildFromDirectory(ScannerFixtureRoot, ImagePathError));
    TestEqual(
        TEXT("Filesystem scan publishes both authored resources"),
        ScannedCatalog->GetEntries().Num(),
        2);
    if (ScannedCatalog->GetEntries().Num() == 2)
    {
        TestEqual(
            TEXT("Filesystem resource keeps the derived ID"),
            ScannedCatalog->GetEntries()[0].ResourceId,
            FString(TEXT("core:resource.image.character_portrait")));
        TestEqual(
            TEXT("Plain PNG derives its fixed aspect ratio"),
            ScannedCatalog->GetEntries()[0].FixedAspectRatio,
            2.0f / 3.0f);
        TestEqual(
            TEXT("Nine-slice suffix selects nine-slice mode"),
            ScannedCatalog->GetEntries()[1].RenderMode,
            EGV2ImageRenderMode::NineSlice);
        TestEqual(
            TEXT("Nine-slice top marker derives the left border"),
            static_cast<float>(ScannedCatalog->GetEntries()[1].NineSliceBorderPixels.Left),
            1.0f);
        TestEqual(
            TEXT("Nine-slice left marker derives the top border"),
            static_cast<float>(ScannedCatalog->GetEntries()[1].NineSliceBorderPixels.Top),
            1.0f);
        UTexture2D* NineSliceTexture = ScannedCatalog->GetEntries()[1].Texture.Get();
        TestNotNull(TEXT("Nine-slice scanner creates a cropped runtime texture"), NineSliceTexture);
        if (NineSliceTexture != nullptr)
        {
            TestEqual(TEXT("Nine-slice marker border is cropped from width"), NineSliceTexture->GetSizeX(), 4);
            TestEqual(TEXT("Nine-slice marker border is cropped from height"), NineSliceTexture->GetSizeY(), 4);
        }
    }

    FGV2ResolvedImageResource FirstPortraitResolve;
    FGV2ResolvedImageResource SecondPortraitResolve;
    TestTrue(
        TEXT("Prepared fixed-aspect resource resolves from the catalog lookup"),
        ScannedCatalog->Resolve(
            TEXT("core:resource.image.character_portrait"),
            FirstPortraitResolve,
            ImagePathError));
    TestTrue(
        TEXT("Repeated resolve returns the prepared fixed-aspect resource"),
        ScannedCatalog->Resolve(
            TEXT("core:resource.image.character_portrait"),
            SecondPortraitResolve,
            ImagePathError));
    TestEqual(
        TEXT("Repeated resolve preserves the prepared brush resource object"),
        FirstPortraitResolve.Brush.GetResourceObject(),
        SecondPortraitResolve.Brush.GetResourceObject());
    TestEqual(
        TEXT("Repeated resolve preserves the prepared brush size"),
        FirstPortraitResolve.Brush.ImageSize,
        SecondPortraitResolve.Brush.ImageSize);

    FGV2ResolvedImageResource ScannedPanel;
    TestTrue(
        TEXT("Prepared nine-slice resource resolves from the catalog lookup"),
        ScannedCatalog->Resolve(
            TEXT("core:resource.image.panel"),
            ScannedPanel,
            ImagePathError));
    TestEqual(
        TEXT("Prepared nine-slice lookup retains box drawing"),
        ScannedPanel.Brush.DrawAs,
        ESlateBrushDrawType::Box);
    TestFalse(
        TEXT("Invalid resource ID is rejected before lookup"),
        ScannedCatalog->Resolve(
            TEXT("Core:resource.image.character_portrait"),
            ScannedPanel,
            ImagePathError));
    TestFalse(
        TEXT("Unknown canonical resource ID is rejected by lookup"),
        ScannedCatalog->Resolve(
            TEXT("core:resource.image.missing"),
            ScannedPanel,
            ImagePathError));
    IFileManager::Get().DeleteDirectory(*ScannerFixtureRoot, false, true);

    FString ConfiguredCatalogError;
    UGV2ImageResourceCatalog* ConfiguredImageCatalog =
        GV2PresentationTestFixtures::BuildGameDataImageCatalog(ConfiguredCatalogError);
    TestNotNull(
        *FString::Printf(TEXT("Configured image catalog builds [Error: %s]"), *ConfiguredCatalogError),
        ConfiguredImageCatalog);
    if (ConfiguredImageCatalog != nullptr)
    {
        FGV2ResolvedImageResource PaperTile;
        TestTrue(
            TEXT("Authored paper tile resolves by suffix-free resource_id"),
            ConfiguredImageCatalog->Resolve(
                TEXT("core:resource.ui.old_paper_tile_256"),
                PaperTile,
                ImagePathError));
        TestEqual(
            TEXT("Authored paper resource uses tile mode"),
            PaperTile.RenderMode,
            EGV2ImageRenderMode::Tile);
        TestEqual(
            TEXT("Authored paper tile keeps its decoded logical width"),
            static_cast<float>(PaperTile.Brush.ImageSize.X),
            256.0f);
        TestEqual(
            TEXT("Authored paper tile keeps its decoded logical height"),
            static_cast<float>(PaperTile.Brush.ImageSize.Y),
            256.0f);
        TestEqual(
            TEXT("Authored paper resource tiles in both axes"),
            PaperTile.Brush.Tiling,
            ESlateBrushTileType::Both);
    }

    UTexture2D* ImageFixtureTexture = UTexture2D::CreateTransient(64, 64);
    TestNotNull(TEXT("Image resource fixture texture is available"), ImageFixtureTexture);
    if (ImageFixtureTexture != nullptr)
    {
        FGV2ImageResourceDefinition FixedAspectDefinition;
        FixedAspectDefinition.ResourceId = TEXT("core:resource.image.test_portrait");
        FixedAspectDefinition.Texture = ImageFixtureTexture;
        FixedAspectDefinition.RenderMode = EGV2ImageRenderMode::FixedAspect;
        FixedAspectDefinition.FixedAspectRatio = 2.0f / 3.0f;

        FString ImageResourceError;
        FGV2ResolvedImageResource ResolvedImage;
        TestTrue(
            TEXT("fixed_aspect image resource resolves"),
            UGV2ImageResourceCatalog::ResolveDefinition(
                FixedAspectDefinition,
                ResolvedImage,
                ImageResourceError));
        TestEqual(
            TEXT("fixed_aspect resource uses an ordinary image brush"),
            ResolvedImage.Brush.DrawAs,
            ESlateBrushDrawType::Image);
        TestEqual(
            TEXT("fixed_aspect resource preserves declared ratio"),
            ResolvedImage.FixedAspectRatio,
            2.0f / 3.0f);

        FGV2ImageResourceDefinition NineSliceDefinition = FixedAspectDefinition;
        NineSliceDefinition.ResourceId = TEXT("core:resource.surface.test_panel");
        NineSliceDefinition.RenderMode = EGV2ImageRenderMode::NineSlice;
        NineSliceDefinition.NineSliceBorderPixels = FMargin(8.0f);
        TestTrue(
            TEXT("nine_slice image resource resolves"),
            UGV2ImageResourceCatalog::ResolveDefinition(
                NineSliceDefinition,
                ResolvedImage,
                ImageResourceError));
        TestEqual(
            TEXT("nine_slice resource produces a box brush"),
            ResolvedImage.Brush.DrawAs,
            ESlateBrushDrawType::Box);
        TestEqual(
            TEXT("nine_slice borders normalize against texture width"),
            static_cast<float>(ResolvedImage.Brush.Margin.Left),
            0.125f);

        FGV2ImageResourceDefinition TileDefinition = FixedAspectDefinition;
        TileDefinition.ResourceId = TEXT("core:resource.pattern.test_background");
        TileDefinition.RenderMode = EGV2ImageRenderMode::Tile;
        TileDefinition.TileSize = FVector2D(24.0f, 40.0f);
        TestTrue(
            TEXT("tile image resource resolves"),
            UGV2ImageResourceCatalog::ResolveDefinition(
                TileDefinition,
                ResolvedImage,
                ImageResourceError));
        TestEqual(
            TEXT("tile resource repeats on both axes"),
            ResolvedImage.Brush.Tiling,
            ESlateBrushTileType::Both);
        TestEqual(
            TEXT("tile resource retains logical repeat width"),
            static_cast<float>(ResolvedImage.Brush.ImageSize.X),
            24.0f);
        TestEqual(
            TEXT("tile resource retains logical repeat height"),
            static_cast<float>(ResolvedImage.Brush.ImageSize.Y),
            40.0f);

        FixedAspectDefinition.FixedAspectRatio = 0.0f;
        TestFalse(
            TEXT("fixed_aspect resource rejects a non-positive ratio"),
            UGV2ImageResourceCatalog::ValidateDefinition(
                FixedAspectDefinition,
                ImageResourceError));
        NineSliceDefinition.NineSliceBorderPixels = FMargin(32.0f, 1.0f, 32.0f, 1.0f);
        TestFalse(
            TEXT("nine_slice resource rejects a collapsed center"),
            UGV2ImageResourceCatalog::ResolveDefinition(
                NineSliceDefinition,
                ResolvedImage,
                ImageResourceError));
    }

    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    FARFilter UiAssetFilter;
    UiAssetFilter.PackagePaths.Add(TEXT("/Game/UI"));
    UiAssetFilter.PackagePaths.Add(TEXT("/Game/TextSystem/UI"));
    UiAssetFilter.PackagePaths.Add(TEXT("/Game/RH/UI"));
    UiAssetFilter.bRecursivePaths = true;
    TArray<FAssetData> UiAssets;
    AssetRegistryModule.Get().GetAssets(UiAssetFilter, UiAssets);

    // DCA-17: the leaf-component contract table (asset -> expected native parent). This pairing
    // is genuine domain knowledge and stays hand-authored, but every OTHER discovered WBP_* is
    // now required to fall into a checkable, reflective bucket in the loop below -- a generic
    // declared composite/screen (UGV2DeclaredCompositeWidgetBase/UGV2ScreenWidgetBase, since
    // DeclaredCompositeAdoption's whole point is that these need no bespoke C++ of their own) or
    // a Designer-configured sibling of an already-vetted leaf's native parent (e.g.
    // WBP_ListView_Wrap/WrapButtons sharing WBP_ListView's UGV2ListViewWidgetBase) -- instead of
    // silently vanishing from a hand-adjusted total.
    struct FComponentContract
    {
        const TCHAR* ClassPath;
        UClass* NativeParent;
    };
    const FComponentContract Components[] = {
        {TEXT("/Game/UI/Widgets/WBP_Text.WBP_Text_C"), UGV2TextWidgetBase::StaticClass()},
        {TEXT("/Game/TextSystem/UI/Widgets/WBP_RichText.WBP_RichText_C"), UGV2RichTextWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_Image.WBP_Image_C"), UGV2ImageWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_Button.WBP_Button_C"), UGV2ButtonWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_Checkbox.WBP_Checkbox_C"), UGV2CheckboxWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_InputField.WBP_InputField_C"), UGV2InputFieldWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_DropdownSelect.WBP_DropdownSelect_C"), UGV2DropdownSelectWidgetBase::StaticClass()},
        {TEXT("/Game/TextSystem/UI/Widgets/WBP_ButtonList.WBP_ButtonList_C"), UGV2ButtonListWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_ProgressBar.WBP_ProgressBar_C"), UGV2ProgressBarWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_Separator.WBP_Separator_C"), UGV2SeparatorWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_LoadingIndicator.WBP_LoadingIndicator_C"), UGV2LoadingIndicatorWidgetBase::StaticClass()},
        {TEXT("/Game/TextSystem/UI/Widgets/WBP_RichTextPopover.WBP_RichTextPopover_C"), UGV2RichTextPopoverWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_Icon.WBP_Icon_C"), UGV2IconWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_Panel.WBP_Panel_C"), UGV2PanelWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_ScrollArea.WBP_ScrollArea_C"), UGV2ScrollAreaWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_ListView.WBP_ListView_C"), UGV2ListViewWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Widgets/WBP_TabContainer.WBP_TabContainer_C"), UGV2TabContainerWidgetBase::StaticClass()},
        {TEXT("/Game/TextSystem/UI/Widgets/WBP_Modal.WBP_Modal_C"), UGV2ModalWidgetBase::StaticClass()},
        {TEXT("/Game/TextSystem/UI/Widgets/WBP_Portrait.WBP_Portrait_C"), UGV2PortraitWidgetBase::StaticClass()},
        {TEXT("/Game/UI/Shell/WBP_GameShell.WBP_GameShell_C"), UGV2GameShellWidgetBase::StaticClass()},
    };

    TSet<FString> ComponentContractClassPaths;
    TSet<UClass*> ComponentContractNativeParents;
    for (const FComponentContract& Component : Components)
    {
        ComponentContractClassPaths.Add(Component.ClassPath);
        ComponentContractNativeParents.Add(Component.NativeParent);
    }

    int32 WidgetBlueprintCount = 0;
    int32 ReconciledWidgetBlueprintCount = 0;
    for (const FAssetData& Asset : UiAssets)
    {
        const FString AssetName = Asset.AssetName.ToString();
        if (!AssetName.StartsWith(TEXT("WBP_")))
        {
            continue;
        }
        ++WidgetBlueprintCount;
        const FString GeneratedClassPath = FString::Printf(
            TEXT("%s.%s_C"),
            *Asset.PackageName.ToString(),
            *AssetName);
        UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, *GeneratedClassPath);
        TestNotNull(
            *FString::Printf(TEXT("Current WBP has a loadable generated class: %s"), *AssetName),
            WidgetClass);
        if (WidgetClass == nullptr)
        {
            continue;
        }

        const bool bIsComponentContractLeaf = ComponentContractClassPaths.Contains(GeneratedClassPath);
        const bool bIsGenericDeclaredCompositeOrScreen =
            WidgetClass->IsChildOf(UGV2DeclaredCompositeWidgetBase::StaticClass())
            || WidgetClass->IsChildOf(UGV2ScreenWidgetBase::StaticClass());
        bool bSharesComponentContractNativeParent = false;
        for (UClass* CoveredParent : ComponentContractNativeParents)
        {
            if (WidgetClass->IsChildOf(CoveredParent))
            {
                bSharesComponentContractNativeParent = true;
                break;
            }
        }
        if (bIsComponentContractLeaf || bIsGenericDeclaredCompositeOrScreen || bSharesComponentContractNativeParent)
        {
            ++ReconciledWidgetBlueprintCount;
        }
        else
        {
            AddError(FString::Printf(
                TEXT("WBP has no Components[] contract, declared composite/screen base, or shared leaf native parent: %s"),
                *AssetName));
        }

        const UWidgetBlueprintGeneratedClass* GeneratedClass =
            Cast<UWidgetBlueprintGeneratedClass>(WidgetClass);
        if (GeneratedClass == nullptr || GeneratedClass->GetWidgetTreeArchetype() == nullptr)
        {
            continue;
        }

        bool bContainsDirectTextPrimitive = false;
        GeneratedClass->GetWidgetTreeArchetype()->ForEachWidget(
            [&bContainsDirectTextPrimitive](UWidget* Widget)
            {
                bContainsDirectTextPrimitive |= Widget != nullptr
                    && (Widget->IsA<UTextBlock>() || Widget->IsA<URichTextBlock>());
            });
        if (bContainsDirectTextPrimitive)
        {
            // DCA-17: a class property (IGV2TextPipelineHost) replaces the former 9-class
            // IsChildOf enumeration -- see GV2TextPipelineHost.h for what implementing it means.
            TestTrue(
                *FString::Printf(
                    TEXT("Text-bearing WBP must use a Text Pipeline native base: %s"),
                    *AssetName),
                WidgetClass->ImplementsInterface(UGV2TextPipelineHost::StaticClass()));
        }
    }
    TestEqual(
        TEXT("Every discovered WBP is reconciled to a leaf contract, a declared base, or a shared native parent"),
        ReconciledWidgetBlueprintCount,
        WidgetBlueprintCount);
    TestTrue(
        TEXT("Theme provides a visible separator brush"),
        Theme->SeparatorBrush.DrawAs != ESlateBrushDrawType::NoDrawType);
    TestTrue(
        TEXT("Theme provides a visible loading indicator brush"),
        Theme->LoadingIndicatorBrush.DrawAs != ESlateBrushDrawType::NoDrawType);

    GV2PresentationTestFixtures::FScopedTestWorldContext WorldContext;
    UGameInstance* GameInstance = WorldContext.GetGameInstance();
    UWorld* TestWorld = WorldContext.GetWorld();

    for (const FComponentContract& Component : Components)
    {
        UClass* ComponentClass = LoadClass<UUserWidget>(nullptr, Component.ClassPath);
        TestNotNull(*FString::Printf(TEXT("UI component is loadable: %s"), Component.ClassPath), ComponentClass);
        if (ComponentClass == nullptr)
        {
            continue;
        }
        TestTrue(
            *FString::Printf(TEXT("UI component has expected native parent: %s"), Component.ClassPath),
            ComponentClass->IsChildOf(Component.NativeParent));

        UUserWidget* Widget = TestWorld != nullptr
            ? CreateWidget<UUserWidget>(TestWorld, ComponentClass)
            : nullptr;
        TestNotNull(*FString::Printf(TEXT("UI component instantiates: %s"), Component.ClassPath), Widget);
        if (Widget != nullptr)
        {
            if (UGV2RichTextWidgetBase* RichText = Cast<UGV2RichTextWidgetBase>(Widget))
            {
                const GV2PresentationApply::FPreparedRichTextStyle PreparedRichTextStyle =
                    MakePreparedRichTextStyleForTest(*Theme);
                RichText->ApplyRichTextStyleValues(PreparedRichTextStyle);
                UCommonRichTextBlock* RichTextBlock = Cast<UCommonRichTextBlock>(
                    RichText->GetWidgetFromName(TEXT("RichTextBlock")));
                UScrollBox* RichTextScrollBox = Cast<UScrollBox>(
                    RichText->GetWidgetFromName(TEXT("RichTextScrollBox")));
                TestNotNull(TEXT("RichText owns its vertical ScrollBox"), RichTextScrollBox);
                TestNotNull(TEXT("RichText owns its CommonRichTextBlock"), RichTextBlock);
                if (RichTextBlock != nullptr)
                {
                    TestTrue(
                        TEXT("RichText automatically wraps to its allocated width"),
                        RichTextBlock->GetAutoWrapText());
                }
                if (RichTextScrollBox != nullptr)
                {
                    TestEqual(
                        TEXT("RichText overflow scrolls vertically"),
                        RichTextScrollBox->GetOrientation(),
                        EOrientation::Orient_Vertical);
                    RichTextScrollBox->SetScrollOffset(42.0f);
                    const FGV2TextViewModel ReplacementText =
                        MakeResolvedLiteralTextForTest(*Theme, TEXT("Replacement text"));
                    TestTrue(TEXT("Prepared replacement RichText applies"), RichText->ApplyText(ReplacementText));
                    TestEqual(
                        TEXT("Applying replacement RichText resets scroll to the start"),
                        RichTextScrollBox->GetScrollOffset(),
                        0.0f);
                }
                const UCommonTextStyle* RichTextStyle = Theme->RichTextStyle != nullptr
                    ? Cast<UCommonTextStyle>(Theme->RichTextStyle->GetDefaultObject())
                    : nullptr;
                FSlateFontInfo ExpectedFont;
                if (RichTextStyle != nullptr)
                {
                    RichTextStyle->GetFont(ExpectedFont);
                }
                const FSlateFontInfo& InteractiveFont =
                    RichText->ResolveRunTextStyle(TEXT("default"), NAME_None, NAME_None).Font;
                TestEqual(
                    TEXT("Interactive RichText inherits the configured font object"),
                    InteractiveFont.FontObject,
                    ExpectedFont.FontObject);
                TestEqual(
                    TEXT("Interactive RichText inherits the configured typeface"),
                    InteractiveFont.TypefaceFontName,
                    ExpectedFont.TypefaceFontName);
                TestEqual(
                    TEXT("Interactive RichText inherits the configured font size"),
                    InteractiveFont.Size,
                    ExpectedFont.Size);
                }
            }
            if (UGV2RichTextPopoverWidgetBase* Popover =
                    Cast<UGV2RichTextPopoverWidgetBase>(Widget))
            {
                UGV2RichTextWidgetBase* PopoverDescription =
                    Cast<UGV2RichTextWidgetBase>(
                        Popover->GetWidgetFromName(TEXT("DescriptionText")));
                TestNotNull(
                    TEXT("RichText popover composes the reusable RichText component"),
                    PopoverDescription);
                FGV2RichTextHoverViewModel HoverModel;
                HoverModel.Title.Text = FText::FromString(TEXT("Title"));
                HoverModel.Title = MakeResolvedLiteralTextForTest(*Theme, TEXT("Title"));
                HoverModel.Description = MakeResolvedLiteralTextForTest(
                    *Theme,
                    TEXT("A long popover description that must use the shared wrapping and scrolling behavior."));
                TestTrue(
                    TEXT("RichText popover initializes through the reusable component"),
                    Popover->InitializePopover(
                        HoverModel,
                        MakePreparedRichTextStyleForTest(*Theme)));
                if (PopoverDescription != nullptr)
                {
                    UCommonRichTextBlock* PopoverRichText =
                        Cast<UCommonRichTextBlock>(PopoverDescription->GetWidgetFromName(TEXT("RichTextBlock")));
                    UScrollBox* PopoverScrollBox = Cast<UScrollBox>(
                        PopoverDescription->GetWidgetFromName(TEXT("RichTextScrollBox")));
                    TestTrue(
                        TEXT("Popover description inherits automatic wrapping"),
                        PopoverRichText != nullptr && PopoverRichText->GetAutoWrapText());
                    TestNotNull(
                        TEXT("Popover description inherits vertical overflow scrolling"),
                        PopoverScrollBox);
                }
            }
        }

    UClass* TestScreenClass = LoadClass<UUserWidget>(
        nullptr,
        TEXT("/Game/UI/Widgets/WBP_Testscreen.WBP_Testscreen_C"));
    UUserWidget* TestScreen = TestWorld != nullptr && TestScreenClass != nullptr
        ? CreateWidget<UUserWidget>(TestWorld, TestScreenClass)
        : nullptr;
    TestNotNull(TEXT("Test screen with the paper surface instantiates"), TestScreen);
    if (TestScreen != nullptr)
    {
        TestScreen->TakeWidget();
        UGV2ImageWidgetBase* DescriptionBackground = Cast<UGV2ImageWidgetBase>(
            TestScreen->GetWidgetFromName(TEXT("DescriptionBackground")));
        TestNotNull(TEXT("Test screen exposes its WBP_Image description background"), DescriptionBackground);
        if (DescriptionBackground != nullptr)
        {
            // PSC-10C: the widget carries a Blueprint-authored InitialResourceId, and
            // instantiating it -- which runs NativePreConstruct -- must NOT resolve it. A
            // lifecycle callback has no session, so resolving there meant reaching a
            // process-global catalog; the widget now stays on its serialized brush until a
            // prepared operation arrives.
            TestEqual(
                TEXT("PSC-10C: instantiation alone applies no resource, because NativePreConstruct resolves nothing"),
                DescriptionBackground->GetAppliedResourceId(),
                FString());
            TestNotEqual(
                TEXT("PSC-10C: the authoring default is still declared on the widget"),
                DescriptionBackground->GetInitialResourceId(),
                FString());

            // The same widget, inside a prepared subtree: the authoring default is resolved
            // against the session snapshot and applied as an ordinary image-host operation.
            GV2PresentationTestFixtures::FPrepareContextFixture ImageContextFixture;
            FString ImageContextError;
            const bool bImageContextReady = ImageContextFixture.Initialize(ImageContextError);
            TestTrue(
                *FString::Printf(TEXT("Prepare context for the image default builds [Error: %s]"), *ImageContextError),
                bImageContextReady);
            if (bImageContextReady && ImageContextFixture.Get() != nullptr)
            {
                GV2PresentationApply::FGV2PreparedPresentationTransaction ImageTransaction;
                FString ImagePrepareError;
                TestTrue(
                    *FString::Printf(TEXT("PSC-10C: the subtree walk prepares the image default [Error: %s]"), *ImagePrepareError),
                    GV2CentralStylePreparer::PrepareForSubtree(
                        DescriptionBackground, *ImageContextFixture.Get(), ImageTransaction, ImagePrepareError));
                FString ImageApplyError;
                TestTrue(TEXT("PSC-10C: the prepared image transaction applies"),
                    GV2PresentationTestFixtures::ApplyPreparedTransaction(ImageTransaction, ImageApplyError)
                        && GV2PresentationTestFixtures::ApplyPreparedTransaction(ImageTransaction, ImageApplyError));
                TestEqual(
                    TEXT("Description background applies the suffix-free paper resource_id"),
                    DescriptionBackground->GetAppliedResourceId(),
                    FString(TEXT("core:resource.ui.old_paper_tile_256")));
                TestEqual(
                    TEXT("Description background renders as a two-axis tile"),
                    DescriptionBackground->GetImageBrush().Tiling,
                    ESlateBrushTileType::Both);
            }
        }
    }

    return true;
}



IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2UiThemeOwnershipAndTextLengthContract,
    "GV2.Runtime.UI.ThemeOwnershipAndTextLengthContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2UiThemeOwnershipAndTextLengthContract::RunTest(const FString& Parameters)
{
    // =========================================================================
    // UIF-27 & UIF-28: Layer Directory Convention & Screen Registry Gate
    // DCA-18: every expectation below is a position comparison against
    // PackageLoadOrder, computed independently in this test -- never a hardcoded
    // true/false copied from UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace's
    // own branches.
    // =========================================================================
    {
        const TArray<GV2PackageClosure::FEntry> RealClosureEntries = GV2PackageClosure::DiscoverFromGameData();
        const TArray<FString> RealPackageLoadOrder = UGV2ScreenRegistry::GetPackageLoadOrderFromGameData(RealClosureEntries);
        TestEqual(TEXT("GameData package load order resolves exactly core, textsystem, rh"), RealPackageLoadOrder.Num(), 3);

        // PAH-05: content root ownership now comes from each package's own
        // GameData/<id>/package.json5 "ue_content_roots" -- core declares two
        // (/Game/core, /Game/UI), textsystem and rh one each.
        TArray<FGV2ContentRootOwnership> RealOwnership;
        FString OwnershipError;
        TestTrue(
            *FString::Printf(TEXT("GameData content root ownership resolves [Error: %s]"), *OwnershipError),
            UGV2ScreenRegistry::ResolveContentRootOwnershipFromGameData(RealClosureEntries, RealOwnership, OwnershipError));
        TestEqual(TEXT("GameData declares exactly four UE content roots across three packages"), RealOwnership.Num(), 4);

        auto ExpectedAllowed = [](const TArray<FString>& Order, const TArray<FGV2ContentRootOwnership>& Ownership, const FString& ScreenNamespace, const FString& AssetPath) -> bool
        {
            const int32 ScreenIdx = Order.IndexOfByPredicate(
                [&ScreenNamespace](const FString& PackageId) { return PackageId.Equals(ScreenNamespace, ESearchCase::IgnoreCase); });
            if (ScreenIdx == INDEX_NONE)
            {
                return false;
            }
            const FString OwningPackage = UGV2ScreenRegistry::FindOwningPackageForAssetPath(AssetPath, Ownership);
            if (OwningPackage.IsEmpty())
            {
                // PAH-03: unowned /Game/ content is rejected; content outside /Game/
                // entirely is trusted by declared domain. Independently recomputed here,
                // not copied from the production branch it mirrors.
                return UGV2ScreenRegistry::IsTrustedExternalContentDomain(AssetPath);
            }
            const int32 AssetIdx = Order.IndexOfByPredicate(
                [&OwningPackage](const FString& PackageId) { return PackageId.Equals(OwningPackage, ESearchCase::IgnoreCase); });
            return AssetIdx == INDEX_NONE || AssetIdx <= ScreenIdx;
        };

        struct FCase
        {
            FString ScreenNamespace;
            FString AssetPath;
        };
        const FCase Cases[] = {
            {TEXT("core"), TEXT("/Game/UI/Widgets/WBP_Testscreen")},
            {TEXT("core"), TEXT("/Game/core/WBP_CoreScreen")},
            {TEXT("core"), TEXT("/Game/TextSystem/UI/Screens/WBP_Textscreen")},
            {TEXT("core"), TEXT("/Game/RH/UI/Screens/WBP_RHScreen")},
            {TEXT("textsystem"), TEXT("/Game/TextSystem/UI/Screens/WBP_Textscreen")},
            {TEXT("textsystem"), TEXT("/Game/UI/Widgets/WBP_Testscreen")},
            {TEXT("textsystem"), TEXT("/Game/RH/UI/Screens/WBP_RHScreen")},
            {TEXT("rh"), TEXT("/Game/RH/UI/Screens/WBP_RHScreen")},
            {TEXT("rh"), TEXT("/Game/TextSystem/UI/Screens/WBP_Textscreen")},
            {TEXT("rh"), TEXT("/Game/UI/Widgets/WBP_Testscreen")},
        };
        int32 CasesCovered = 0;
        for (const FCase& Case : Cases)
        {
            const bool bExpected = ExpectedAllowed(RealPackageLoadOrder, RealOwnership, Case.ScreenNamespace, Case.AssetPath);
            const bool bActual = UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(
                Case.ScreenNamespace, Case.AssetPath, RealPackageLoadOrder, RealOwnership);
            TestEqual(
                *FString::Printf(TEXT("%s screen vs %s matches the load_index-derived expectation"), *Case.ScreenNamespace, *Case.AssetPath),
                bActual,
                bExpected);
            ++CasesCovered;
        }
        TestEqual(TEXT("Every namespace/asset case above was evaluated"), CasesCovered, static_cast<int32>(UE_ARRAY_COUNT(Cases)));

        // A namespace absent from the pinned closure is rejected, not allowed by default
        // (the old ladder's unconditional trailing `return true` for this exact case).
        TestFalse(
            TEXT("Namespace absent from the pinned closure is rejected"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(
                TEXT("sample"), TEXT("/Game/RH/UI/Screens/WBP_RHScreen"), RealPackageLoadOrder, RealOwnership));

        // PAH-03/05: a /Game/ asset whose root isn't declared by any package in the
        // closure is unowned, not unconstrained -- rejected, not the old ladder's
        // `return true` for an empty FindOwningPackageForAssetPath result.
        TestFalse(
            TEXT("PAH-03: synthetic /Game/ path outside every declared package root is rejected"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(
                TEXT("core"), TEXT("/Game/SyntheticUnownedFeature/WBP_Unowned"), RealPackageLoadOrder, RealOwnership));
        TestTrue(
            TEXT("PAH-03: FindOwningPackageForAssetPath itself returns empty for that path"),
            UGV2ScreenRegistry::FindOwningPackageForAssetPath(TEXT("/Game/SyntheticUnownedFeature/WBP_Unowned"), RealOwnership).IsEmpty());

        // PAH-03: content outside /Game/ entirely (engine-shipped, or an enabled plugin's
        // own content root) has no project-package ownership to violate and is trusted by
        // declared domain, not rejected alongside a genuinely unowned /Game/ root.
        TestTrue(
            TEXT("PAH-03: engine-shipped content path is a trusted external domain"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(
                TEXT("core"), TEXT("/Engine/EditorResources/S_Actor"), RealPackageLoadOrder, RealOwnership));
        TestTrue(
            TEXT("PAH-03: enabled-plugin content path is a trusted external domain"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(
                TEXT("core"), TEXT("/CommonUI/Widgets/WBP_SomePluginWidget"), RealPackageLoadOrder, RealOwnership));
        TestTrue(
            TEXT("IsTrustedExternalContentDomain itself: /Engine/ path"),
            UGV2ScreenRegistry::IsTrustedExternalContentDomain(TEXT("/Engine/EditorResources/S_Actor")));
        TestFalse(
            TEXT("IsTrustedExternalContentDomain itself: a /Game/ path is never a trusted external domain"),
            UGV2ScreenRegistry::IsTrustedExternalContentDomain(TEXT("/Game/SyntheticUnownedFeature/WBP_Unowned")));

        // A package that doesn't exist in today's real closure still works correctly once
        // it's present in PackageLoadOrder -- proving the rule reads positions generically
        // instead of special-casing three known names. Ownership (which package owns
        // /Game/RH/) is unaffected by load order, so RealOwnership is reused as-is.
        const TArray<FString> ExtendedOrder = {TEXT("core"), TEXT("textsystem"), TEXT("rh"), TEXT("modx")};
        TestTrue(
            TEXT("A fourth package appended to the closure can reference the layer directly below it"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(TEXT("modx"), TEXT("/Game/RH/UI/Screens/WBP_RHScreen"), ExtendedOrder, RealOwnership));

        // Reversing the closure's order flips which layer may reference which, proving the
        // decision is read from PackageLoadOrder's positions and not hardcoded by name.
        const TArray<FString> ReversedOrder = {TEXT("rh"), TEXT("textsystem"), TEXT("core")};
        TestTrue(
            TEXT("Under a reversed closure, core (now highest) can reference rh (now lowest)"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(
                TEXT("core"), TEXT("/Game/RH/UI/Screens/WBP_RHScreen"), ReversedOrder, RealOwnership));
        TestFalse(
            TEXT("Under a reversed closure, rh (now lowest) cannot reference core (now highest)"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(
                TEXT("rh"), TEXT("/Game/UI/Widgets/WBP_Testscreen"), ReversedOrder, RealOwnership));

        // End-to-end: a Screen Registry entry violating layer ownership is still rejected.
        FGV2ScreenRegistryEntry BadEntry;
        BadEntry.ScreenId = TEXT("core:screen.bad_ref");
        BadEntry.Layer = TEXT("location_content");
        BadEntry.WidgetClass = TSoftClassPtr<UGV2ScreenWidgetBase>(FSoftObjectPath(TEXT("/Game/TextSystem/UI/Screens/WBP_Textscreen.WBP_Textscreen_C")));
        TestFalse(
            TEXT("Core screen referencing TextSystem is rejected"),
            UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(
                TEXT("core"), BadEntry.WidgetClass.ToSoftObjectPath().ToString(), RealPackageLoadOrder, RealOwnership));

        // PAH-05: a synthetic fourth package declares its OWN content root -- understood
        // by BuildContentRootOwnership/FindOwningPackageForAssetPath purely from this
        // data, with zero Source/ changes (no fifth entry added to any hardcoded table,
        // because none exists anymore).
        TArray<FGV2ContentRootOwnership> SyntheticOwnership;
        FString SyntheticError;
        const TArray<FGV2DeclaredPackageRoots> SyntheticDeclared = {
            FGV2DeclaredPackageRoots{TEXT("core"), {TEXT("/Game/core"), TEXT("/Game/UI")}},
            FGV2DeclaredPackageRoots{TEXT("modx"), {TEXT("/Game/ModX")}},
        };
        TestTrue(
            TEXT("PAH-05: a synthetic fourth package's own declared root resolves without any Source/ change"),
            UGV2ScreenRegistry::BuildContentRootOwnership(SyntheticDeclared, SyntheticOwnership, SyntheticError));
        TestEqual(
            TEXT("PAH-05: the synthetic fourth package's asset resolves to its own package_id"),
            UGV2ScreenRegistry::FindOwningPackageForAssetPath(TEXT("/Game/ModX/Widgets/WBP_ModXScreen"), SyntheticOwnership),
            FString(TEXT("modx")));

        // PAH-05: two packages declaring the same (or a nested) root is a build error,
        // rejected outright -- never resolved in favor of the more specific root.
        TArray<FGV2ContentRootOwnership> OverlappingOwnership;
        FString OverlapError;
        const TArray<FGV2DeclaredPackageRoots> OverlappingDeclared = {
            FGV2DeclaredPackageRoots{TEXT("core"), {TEXT("/Game/UI")}},
            FGV2DeclaredPackageRoots{TEXT("modx"), {TEXT("/Game/UI/Widgets")}},
        };
        TestFalse(
            TEXT("PAH-05: a nested/overlapping root declared by a different package is rejected"),
            UGV2ScreenRegistry::BuildContentRootOwnership(OverlappingDeclared, OverlappingOwnership, OverlapError));
        TestTrue(TEXT("PAH-05: overlap rejection clears any partial ownership result"), OverlappingOwnership.IsEmpty());
        TestFalse(TEXT("PAH-05: overlap rejection names the conflict"), OverlapError.IsEmpty());

        // Same package declaring the same root twice is not a conflict with itself.
        TArray<FGV2ContentRootOwnership> SamePackageOwnership;
        FString SamePackageError;
        const TArray<FGV2DeclaredPackageRoots> SamePackageDeclared = {
            FGV2DeclaredPackageRoots{TEXT("core"), {TEXT("/Game/core"), TEXT("/Game/core/Sub")}},
        };
        TestTrue(
            TEXT("PAH-05: a package's own nested root does not conflict with itself"),
            UGV2ScreenRegistry::BuildContentRootOwnership(SamePackageDeclared, SamePackageOwnership, SamePackageError));
    }

    // =========================================================================
    // UIF-29: Core Minimal Theme & Emergency Fallback Strings
    // =========================================================================
    {
        UGV2UiTheme* MinimalTheme = UGV2UiTheme::GetCoreMinimalTheme();
        TestNotNull(TEXT("Core minimal theme is available"), MinimalTheme);

        if (MinimalTheme != nullptr)
        {
            TestEqual(TEXT("Default style token is 'default'"), MinimalTheme->DefaultTextStyleToken, FName("default"));
            TestTrue(TEXT("Minimal theme contains default text size"), MinimalTheme->TextSizeTokens.Contains(TEXT("default")));
            TestTrue(TEXT("Minimal theme contains title text size"), MinimalTheme->TextSizeTokens.Contains(TEXT("title")));
            TestTrue(TEXT("Minimal theme contains default text color"), MinimalTheme->TextColorTokens.Contains(TEXT("default")));
            TestTrue(TEXT("Minimal theme contains error text color"), MinimalTheme->TextColorTokens.Contains(TEXT("error")));

            // Check emergency fallback strings in catalog
            TestTrue(TEXT("Emergency error title present"), MinimalTheme->TextCatalog.Contains(TEXT("core:text.screen.error.title")));
            TestTrue(TEXT("Emergency error description present"), MinimalTheme->TextCatalog.Contains(TEXT("core:text.screen.error.description")));
            TestTrue(TEXT("Emergency loading title present"), MinimalTheme->TextCatalog.Contains(TEXT("core:text.screen.loading.title")));
            TestTrue(TEXT("Emergency recovery title present"), MinimalTheme->TextCatalog.Contains(TEXT("core:text.screen.recovery.title")));

            // Resolve text through pipeline with minimal theme
            FGV2TextViewModel ResolvedTitle;
            FGV2TextViewModel ResolvedDesc;
            FString Error;
            TestTrue(
                TEXT("Resolve emergency recovery title via MinimalTheme"),
                UGV2TextPipeline::ResolveForAutomationTest(MinimalTheme, TEXT("core:text.screen.recovery.title"), {}, FName("title"), ResolvedTitle, Error));
            TestEqual(TEXT("Recovery title text matches"), ResolvedTitle.Text.ToString(), TEXT("Recovery"));

            TestTrue(
                TEXT("Resolve emergency error description via MinimalTheme"),
                UGV2TextPipeline::ResolveForAutomationTest(MinimalTheme, TEXT("core:text.screen.error.description"), {}, FName("default"), ResolvedDesc, Error));
            TestEqual(TEXT("Error description text matches"), ResolvedDesc.Text.ToString(), TEXT("An unexpected error has occurred."));

            // Verify UE-native recovery screen widget initialization using resolved fallback strings
            UGV2RecoveryScreenWidget* RecoveryScreen = NewObject<UGV2RecoveryScreenWidget>(
                GetTransientPackage(),
                UGV2RecoveryScreenWidget::StaticClass());
            TestNotNull(TEXT("Recovery screen widget instantiated"), RecoveryScreen);
            if (RecoveryScreen != nullptr)
            {
                TestTrue(
                    TEXT("Initialize recovery screen with resolved emergency strings"),
                    RecoveryScreen->InitializeRecoveryScreen(ResolvedTitle.Text.ToString(), ResolvedDesc.Text.ToString()));
                TestEqual(TEXT("Recovery screen title matches"), RecoveryScreen->GetTitle(), TEXT("Recovery"));
                TestEqual(TEXT("Recovery screen message matches"), RecoveryScreen->GetMessage(), TEXT("An unexpected error has occurred."));
            }
        }
    }

    // =========================================================================
    // UIF-30: Text Length Resilience & Automatic Overflow Handling
    // =========================================================================
    {
        UGV2UiTheme* Theme = UGV2UiTheme::GetCoreMinimalTheme();
        TestNotNull(TEXT("Theme is valid for text length test"), Theme);

        // Verify text scaling evaluated on multiple heights
        const float Scale720 = Theme->EvaluateTextScale(720.0f);
        const float Scale1080 = Theme->EvaluateTextScale(1080.0f);
        const float Scale2160 = Theme->EvaluateTextScale(2160.0f);

        TestTrue(TEXT("Text scale on 720p is ~0.85"), FMath::IsNearlyEqual(Scale720, 0.85f, 0.05f));
        TestTrue(TEXT("Text scale on 1080p is 1.0"), FMath::IsNearlyEqual(Scale1080, 1.0f, 0.01f));
        TestTrue(TEXT("Text scale on 2160p is ~1.60"), FMath::IsNearlyEqual(Scale2160, 1.60f, 0.05f));

        // Verify minimum readable font size guarantee
        const float EffectiveSize720 = Theme->GetEffectiveFontSize(TEXT("small"), 720.0f);
        TestTrue(TEXT("Effective font size never drops below MinReadableFontSize"), EffectiveSize720 >= Theme->MinReadableFontSize);
    }

    return true;
}



#endif // WITH_DEV_AUTOMATION_TESTS
