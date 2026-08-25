#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/GV2TextPipeline.h"
#include "UI/GV2UiTheme.h"
#include "UI/GV2TextWidgetBase.h"
#include "UI/GV2RichTextWidgetBase.h"
#include "UI/GV2ButtonWidgetBase.h"
#include "UI/GV2InputFieldWidgetBase.h"
#include "UI/GV2DropdownSelectWidgetBase.h"
#include "CommonTextBlock.h"
#include "CommonRichTextBlock.h"
#include "Components/EditableTextBox.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2WidgetSemanticFontSizeContractTests,
    "GV2.Runtime.UIKit.WidgetSemanticFontSizeContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2WidgetSemanticFontSizeContractTests::RunTest(const FString& Parameters)
{
    UGV2UiTheme* Theme = const_cast<UGV2UiTheme*>(UGV2UiThemeSettings::GetConfiguredTheme());
    TestNotNull(TEXT("Configured theme is valid"), Theme);
    if (Theme == nullptr)
    {
        return false;
    }

    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
    TestNotNull(TEXT("TestWorld created"), TestWorld);
    if (TestWorld == nullptr)
    {
        return false;
    }

    FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
    WorldContext.SetCurrentWorld(TestWorld);

    // Load or instantiate all 5 consumer widget types
    UClass* TextClass = LoadClass<UGV2TextWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Text.WBP_Text_C"));
    UClass* RichTextClass = LoadClass<UGV2RichTextWidgetBase>(nullptr, TEXT("/Game/TextSystem/UI/Widgets/WBP_RichText.WBP_RichText_C"));
    UClass* ButtonClass = LoadClass<UGV2ButtonWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_Button.WBP_Button_C"));
    UClass* InputClass = LoadClass<UGV2InputFieldWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_InputField.WBP_InputField_C"));
    UClass* DropdownClass = LoadClass<UGV2DropdownSelectWidgetBase>(nullptr, TEXT("/Game/UI/Widgets/WBP_DropdownSelect.WBP_DropdownSelect_C"));

    UGV2TextWidgetBase* TextWidget = TextClass != nullptr
        ? CreateWidget<UGV2TextWidgetBase>(TestWorld, TextClass)
        : NewObject<UGV2TextWidgetBase>(TestWorld);
    if (TextWidget != nullptr && TextWidget->GetTextBlock() == nullptr)
    {
        UCommonTextBlock* TextBlock = NewObject<UCommonTextBlock>(TextWidget);
        if (FProperty* Prop = UGV2TextWidgetBase::StaticClass()->FindPropertyByName(TEXT("TextBlock")))
        {
            Prop->SetValue_InContainer(TextWidget, TextBlock);
        }
    }

    UGV2RichTextWidgetBase* RichTextWidget = RichTextClass != nullptr
        ? CreateWidget<UGV2RichTextWidgetBase>(TestWorld, RichTextClass)
        : NewObject<UGV2RichTextWidgetBase>(TestWorld);
    if (RichTextWidget != nullptr && RichTextWidget->GetRichTextBlock() == nullptr)
    {
        UCommonRichTextBlock* RichTextBlock = NewObject<UCommonRichTextBlock>(RichTextWidget);
        if (FProperty* Prop = UGV2RichTextWidgetBase::StaticClass()->FindPropertyByName(TEXT("RichTextBlock")))
        {
            Prop->SetValue_InContainer(RichTextWidget, RichTextBlock);
        }
    }

    UGV2ButtonWidgetBase* ButtonWidget = ButtonClass != nullptr
        ? CreateWidget<UGV2ButtonWidgetBase>(TestWorld, ButtonClass)
        : NewObject<UGV2ButtonWidgetBase>(TestWorld);
    if (ButtonWidget != nullptr && ButtonWidget->GetLabelText() == nullptr)
    {
        UCommonTextBlock* LabelText = NewObject<UCommonTextBlock>(ButtonWidget);
        if (FProperty* Prop = UGV2ButtonWidgetBase::StaticClass()->FindPropertyByName(TEXT("LabelText")))
        {
            Prop->SetValue_InContainer(ButtonWidget, LabelText);
        }
    }

    UGV2InputFieldWidgetBase* InputFieldWidget = InputClass != nullptr
        ? CreateWidget<UGV2InputFieldWidgetBase>(TestWorld, InputClass)
        : NewObject<UGV2InputFieldWidgetBase>(TestWorld);
    if (InputFieldWidget != nullptr)
    {
        if (InputFieldWidget->GetEditableTextBox() == nullptr)
        {
            UEditableTextBox* EditableBox = NewObject<UEditableTextBox>(InputFieldWidget);
            if (FProperty* Prop = UGV2InputFieldWidgetBase::StaticClass()->FindPropertyByName(TEXT("EditableTextBox")))
            {
                Prop->SetValue_InContainer(InputFieldWidget, EditableBox);
            }
        }
        if (InputFieldWidget->GetLabelText() == nullptr)
        {
            UCommonTextBlock* LabelText = NewObject<UCommonTextBlock>(InputFieldWidget);
            if (FProperty* Prop = UGV2InputFieldWidgetBase::StaticClass()->FindPropertyByName(TEXT("LabelText")))
            {
                Prop->SetValue_InContainer(InputFieldWidget, LabelText);
            }
        }
    }

    UGV2DropdownSelectWidgetBase* DropdownWidget = DropdownClass != nullptr
        ? CreateWidget<UGV2DropdownSelectWidgetBase>(TestWorld, DropdownClass)
        : NewObject<UGV2DropdownSelectWidgetBase>(TestWorld);
    if (DropdownWidget != nullptr && DropdownWidget->GetHeaderButton() == nullptr)
    {
        UGV2ButtonWidgetBase* HeaderBtn = ButtonClass != nullptr
            ? CreateWidget<UGV2ButtonWidgetBase>(TestWorld, ButtonClass)
            : NewObject<UGV2ButtonWidgetBase>(DropdownWidget);
        if (HeaderBtn != nullptr && HeaderBtn->GetLabelText() == nullptr)
        {
            UCommonTextBlock* HeaderLabel = NewObject<UCommonTextBlock>(HeaderBtn);
            if (FProperty* Prop = UGV2ButtonWidgetBase::StaticClass()->FindPropertyByName(TEXT("LabelText")))
            {
                Prop->SetValue_InContainer(HeaderBtn, HeaderLabel);
            }
        }
        if (FProperty* Prop = UGV2DropdownSelectWidgetBase::StaticClass()->FindPropertyByName(TEXT("HeaderButton")))
        {
            Prop->SetValue_InContainer(DropdownWidget, HeaderBtn);
        }
    }

    TestNotNull(TEXT("Text widget created with renderer control"), TextWidget ? TextWidget->GetTextBlock() : nullptr);
    TestNotNull(TEXT("RichText widget created with renderer control"), RichTextWidget ? RichTextWidget->GetRichTextBlock() : nullptr);
    TestNotNull(TEXT("Button widget created with renderer control"), ButtonWidget ? ButtonWidget->GetLabelText() : nullptr);
    TestNotNull(TEXT("InputField widget created with renderer control"), InputFieldWidget ? InputFieldWidget->GetEditableTextBox() : nullptr);
    TestNotNull(TEXT("DropdownSelect widget created with renderer control"), DropdownWidget && DropdownWidget->GetHeaderButton() ? DropdownWidget->GetHeaderButton()->GetLabelText() : nullptr);

    const FName SemanticTokens[] = {
        FName(TEXT("title")),
        FName(TEXT("body")),
        FName(TEXT("small"))
    };

    TMap<FName, float> MeasuredTextSizes;

    if (TextWidget && TextWidget->GetTextBlock()
        && RichTextWidget && RichTextWidget->GetRichTextBlock()
        && ButtonWidget && ButtonWidget->GetLabelText()
        && InputFieldWidget && InputFieldWidget->GetEditableTextBox()
        && DropdownWidget && DropdownWidget->GetHeaderButton() && DropdownWidget->GetHeaderButton()->GetLabelText())
    {
        // No game viewport exists in a test world, so the production path resolves the
        // theme reference height. Driving that height exercises the DPI-dependent path
        // of every consumer instead of pinning the check to a single resolution.
        const float OriginalReferenceHeight = Theme->ReferenceViewportHeight;
        const float VerifiedHeights[] = { 720.0f, 1080.0f, 1440.0f, 2160.0f };

        for (const float ViewportHeight : VerifiedHeights)
        {
        Theme->ReferenceViewportHeight = ViewportHeight;
        const FString HeightTag = FString::Printf(TEXT("%dp"), FMath::RoundToInt(ViewportHeight));
        for (const FName& Token : SemanticTokens)
        {
            const float ExpectedSize = Theme->GetEffectiveFontSize(Token, ViewportHeight);
            TestTrue(*FString::Printf(TEXT("[%s][%s] Expected size is positive"), *HeightTag, *Token.ToString()), ExpectedSize > 0.0f);

            // 1. Text widget: Apply via production path and read renderer control
            FGV2TextViewModel TextModel;
            TextModel.Text = FText::FromString(TEXT("Sample Text"));
            TextModel.StyleToken = Token;
            TestTrue(*FString::Printf(TEXT("[%s][%s] ApplyText succeeded"), *HeightTag, *Token.ToString()), TextWidget->ApplyText(TextModel));
            IGV2UiStyleConsumer::Execute_ApplyCentralStyle(TextWidget);
            const float ActualTextSize = TextWidget->GetTextBlock()->GetFont().Size;
            MeasuredTextSizes.Add(Token, ActualTextSize);

            // 2. RichText widget: Apply via production path and read renderer control
            FGV2TextViewModel RichModel;
            RichModel.Text = FText::FromString(TEXT("Sample Rich Text"));
            RichModel.StyleToken = Token;
            RichTextWidget->ApplyText(RichModel);
            IGV2UiStyleConsumer::Execute_ApplyCentralStyle(RichTextWidget);
            const float ActualRichTextSize = RichTextWidget->GetRichTextBlock()->GetCurrentDefaultTextStyle().Font.Size;

            // 3. Button widget: Apply via production path and read renderer control
            FGV2TextViewModel ButtonText;
            ButtonText.Text = FText::FromString(TEXT("Sample Button"));
            ButtonText.StyleToken = Token;
            ButtonWidget->SetKey(TEXT("btn_test"));
            ButtonWidget->SetBindingHandle(FGV2UiBindingHandle::Create(TEXT("core:command.test")));
            ButtonWidget->ApplyText(ButtonText);
            IGV2UiStyleConsumer::Execute_ApplyCentralStyle(ButtonWidget);
            const float ActualButtonSize = ButtonWidget->GetLabelText()->GetFont().Size;

            // 4. InputField widget: Apply via production path and read renderer control
            FGV2TextViewModel InputText;
            InputText.Text = FText::FromString(TEXT("Sample Input"));
            InputText.StyleToken = Token;
            InputFieldWidget->SetKey(TEXT("input_test"));
            InputFieldWidget->SetBindingHandle(FGV2UiBindingHandle::Create(TEXT("core:command.test")));
            InputFieldWidget->ApplyText(InputText);
            IGV2UiStyleConsumer::Execute_ApplyCentralStyle(InputFieldWidget);
            const float ActualInputSize = InputFieldWidget->GetEditableTextBox()->WidgetStyle.TextStyle.Font.Size;

            // 5. DropdownSelect widget: Apply via production path and read renderer control
            FGV2TextViewModel DropdownPlaceholder;
            DropdownPlaceholder.Text = FText::FromString(TEXT("Select item"));
            DropdownPlaceholder.StyleToken = Token;
            DropdownWidget->ApplyPlaceholderText(DropdownPlaceholder);
            IGV2UiStyleConsumer::Execute_ApplyCentralStyle(DropdownWidget);
            const float ActualDropdownSize = DropdownWidget->GetHeaderButton()->GetLabelText()->GetFont().Size;

            // Verify actual renderer font size matches ExpectedSize
            TestEqual(*FString::Printf(TEXT("[%s][%s] Text renderer font size matches expected"), *HeightTag, *Token.ToString()),
                ActualTextSize, ExpectedSize);

            // Verify font size parity across all 5 widget renderer controls
            TestEqual(*FString::Printf(TEXT("[%s][%s] RichText renderer size equals Text renderer size"), *HeightTag, *Token.ToString()),
                ActualRichTextSize, ActualTextSize);

            TestEqual(*FString::Printf(TEXT("[%s][%s] Button renderer size equals Text renderer size"), *HeightTag, *Token.ToString()),
                ActualButtonSize, ActualTextSize);

            TestEqual(*FString::Printf(TEXT("[%s][%s] InputField renderer size equals Text renderer size"), *HeightTag, *Token.ToString()),
                ActualInputSize, ActualTextSize);

            TestEqual(*FString::Printf(TEXT("[%s][%s] DropdownSelect renderer size equals Text renderer size"), *HeightTag, *Token.ToString()),
                ActualDropdownSize, ActualTextSize);
        }

        // Semantic Hierarchy Check on actual measured widget font sizes
        const float* TitleSize = MeasuredTextSizes.Find(FName(TEXT("title")));
        const float* BodySize = MeasuredTextSizes.Find(FName(TEXT("body")));
        const float* SmallSize = MeasuredTextSizes.Find(FName(TEXT("small")));

        if (TitleSize && BodySize && SmallSize)
        {
            TestTrue(*FString::Printf(TEXT("[%s] Hierarchy holds on actual widget renderer: Title > Body"), *HeightTag), *TitleSize > *BodySize);
            TestTrue(*FString::Printf(TEXT("[%s] Hierarchy holds on actual widget renderer: Body > Small"), *HeightTag), *BodySize > *SmallSize);
        }
        MeasuredTextSizes.Reset();
        }

        Theme->ReferenceViewportHeight = OriginalReferenceHeight;
    }

    TestWorld->DestroyWorld(false);
    GEngine->DestroyWorldContext(TestWorld);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
