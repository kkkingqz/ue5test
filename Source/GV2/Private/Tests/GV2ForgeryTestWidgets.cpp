#include "Tests/GV2ForgeryTestWidgets.h"

#include "UI/GV2UiCapability.h"

EGV2ForgeryMode& UGV2ForgeryEntryTestWidget::ModeForNextInstance()
{
    static EGV2ForgeryMode Mode = EGV2ForgeryMode::NoOpConsumer;
    return Mode;
}

void UGV2ForgeryEntryTestWidget::DescribeUiCapabilities(FGV2UiCapabilityBuilder& OutBuilder) const
{
    switch (ModeForNextInstance())
    {
    case EGV2ForgeryMode::NoOpConsumer:
        OutBuilder.AddBinding(TEXT("forgery_binding"), NAME_None);
        break;

    case EGV2ForgeryMode::DetachedRenderer:
        // No child named this exists anywhere on this widget -- the renderer the
        // capability claims to drive was never wired into the tree.
        OutBuilder.AddText(TEXT("forgery_orphan_text"), FName(TEXT("NonexistentDetachedChild")));
        break;

    case EGV2ForgeryMode::UnimplementableKind:
        // StableId with a TargetKind other than "resource" has no probe pair the harness
        // knows how to synthesize (MakeDistinctValuePair only special-cases "resource") --
        // a capability declared for a kind the harness cannot even attempt.
        OutBuilder.AddCustom(
            TEXT("forgery_unimplementable_ref"),
            EGV2PreparedUiValueKind::StableId,
            EGV2UiCapabilityTargetType::RendererControl,
            NAME_None);
        break;
    }
}
