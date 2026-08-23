#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/GV2LayoutConstants.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGV2LayoutConstantsRelationalInvariantsContract,
    "GV2.Runtime.UIKit.LayoutConstantsRelationalInvariants",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGV2LayoutConstantsRelationalInvariantsContract::RunTest(const FString& Parameters)
{
    // 1. Relational validation of active constants
    FString ActiveError;
    const bool bActiveValid = FGV2LayoutConstants::ValidateInvariants(&ActiveError);
    TestTrue(FString::Printf(TEXT("Active layout constants satisfy all relational invariants: %s"), *ActiveError), bActiveValid);

    // 2. Direct relational checks
    TestEqual(
        TEXT("RasterToLayoutScale equals horizontal ratio (3840 / 1920)"),
        FGV2LayoutConstants::RasterAuthoringWidth / FGV2LayoutConstants::VirtualLayoutWidth,
        FGV2LayoutConstants::RasterToLayoutScale);

    TestEqual(
        TEXT("RasterToLayoutScale equals vertical ratio (2160 / 1080)"),
        FGV2LayoutConstants::RasterAuthoringHeight / FGV2LayoutConstants::VirtualLayoutHeight,
        FGV2LayoutConstants::RasterToLayoutScale);

    TestTrue(
        TEXT("Virtual aspect ratio matches StandardAspectRatio (16:9)"),
        FMath::IsNearlyEqual(
            FGV2LayoutConstants::VirtualLayoutWidth / FGV2LayoutConstants::VirtualLayoutHeight,
            FGV2LayoutConstants::StandardAspectRatio,
            1e-4f));

    TestTrue(
        TEXT("Raster aspect ratio matches StandardAspectRatio (16:9)"),
        FMath::IsNearlyEqual(
            FGV2LayoutConstants::RasterAuthoringWidth / FGV2LayoutConstants::RasterAuthoringHeight,
            FGV2LayoutConstants::StandardAspectRatio,
            1e-4f));

    TestTrue(
        TEXT("MinSupportedViewport aspect matches StandardAspectRatio (16:9)"),
        FMath::IsNearlyEqual(
            FGV2LayoutConstants::MinSupportedViewportWidth / FGV2LayoutConstants::MinSupportedViewportHeight,
            FGV2LayoutConstants::StandardAspectRatio,
            1e-4f));

    TestTrue(
        TEXT("MinSupportedViewportWidth does not exceed VirtualLayoutWidth"),
        FGV2LayoutConstants::MinSupportedViewportWidth <= FGV2LayoutConstants::VirtualLayoutWidth);

    TestTrue(
        TEXT("MinSupportedViewportHeight does not exceed VirtualLayoutHeight"),
        FGV2LayoutConstants::MinSupportedViewportHeight <= FGV2LayoutConstants::VirtualLayoutHeight);

    TestTrue(
        TEXT("UltrawideAspectRatio is strictly greater than StandardAspectRatio"),
        FGV2LayoutConstants::UltrawideAspectRatio > FGV2LayoutConstants::StandardAspectRatio);

    // 3. Negative tests: Perturbing any single constant must be detected and rejected by validator
    {
        // 3a. Perturbed RasterAuthoringWidth
        FString Err;
        const bool bValid = FGV2LayoutConstants::ValidateInvariantsValues(
            FGV2LayoutConstants::RasterAuthoringWidth + 100.0f,
            FGV2LayoutConstants::RasterAuthoringHeight,
            FGV2LayoutConstants::VirtualLayoutWidth,
            FGV2LayoutConstants::VirtualLayoutHeight,
            FGV2LayoutConstants::RasterToLayoutScale,
            FGV2LayoutConstants::MinSupportedViewportWidth,
            FGV2LayoutConstants::MinSupportedViewportHeight,
            FGV2LayoutConstants::StandardAspectRatio,
            FGV2LayoutConstants::UltrawideAspectRatio,
            &Err);
        TestFalse(TEXT("Perturbed RasterAuthoringWidth must fail invariant validation"), bValid);
        TestTrue(TEXT("Error message returned on perturbed RasterAuthoringWidth"), !Err.IsEmpty());
    }
    {
        // 3b. Perturbed RasterToLayoutScale
        FString Err;
        const bool bValid = FGV2LayoutConstants::ValidateInvariantsValues(
            FGV2LayoutConstants::RasterAuthoringWidth,
            FGV2LayoutConstants::RasterAuthoringHeight,
            FGV2LayoutConstants::VirtualLayoutWidth,
            FGV2LayoutConstants::VirtualLayoutHeight,
            FGV2LayoutConstants::RasterToLayoutScale * 1.5f,
            FGV2LayoutConstants::MinSupportedViewportWidth,
            FGV2LayoutConstants::MinSupportedViewportHeight,
            FGV2LayoutConstants::StandardAspectRatio,
            FGV2LayoutConstants::UltrawideAspectRatio,
            &Err);
        TestFalse(TEXT("Perturbed RasterToLayoutScale must fail invariant validation"), bValid);
        TestTrue(TEXT("Error message returned on perturbed RasterToLayoutScale"), !Err.IsEmpty());
    }
    {
        // 3c. Perturbed MinSupportedViewport exceeding VirtualLayoutWidth
        FString Err;
        const bool bValid = FGV2LayoutConstants::ValidateInvariantsValues(
            FGV2LayoutConstants::RasterAuthoringWidth,
            FGV2LayoutConstants::RasterAuthoringHeight,
            FGV2LayoutConstants::VirtualLayoutWidth,
            FGV2LayoutConstants::VirtualLayoutHeight,
            FGV2LayoutConstants::RasterToLayoutScale,
            FGV2LayoutConstants::VirtualLayoutWidth + 500.0f,
            FGV2LayoutConstants::MinSupportedViewportHeight,
            FGV2LayoutConstants::StandardAspectRatio,
            FGV2LayoutConstants::UltrawideAspectRatio,
            &Err);
        TestFalse(TEXT("Min viewport exceeding virtual layout must fail validation"), bValid);
        TestTrue(TEXT("Error message returned on oversized min viewport"), !Err.IsEmpty());
    }
    {
        // 3d. Perturbed aspect ratio
        FString Err;
        const bool bValid = FGV2LayoutConstants::ValidateInvariantsValues(
            FGV2LayoutConstants::RasterAuthoringWidth,
            FGV2LayoutConstants::RasterAuthoringHeight,
            FGV2LayoutConstants::VirtualLayoutWidth,
            FGV2LayoutConstants::VirtualLayoutHeight,
            FGV2LayoutConstants::RasterToLayoutScale,
            FGV2LayoutConstants::MinSupportedViewportWidth,
            FGV2LayoutConstants::MinSupportedViewportHeight,
            4.0f / 3.0f, // Mismatched aspect
            FGV2LayoutConstants::UltrawideAspectRatio,
            &Err);
        TestFalse(TEXT("Mismatched aspect ratio must fail validation"), bValid);
        TestTrue(TEXT("Error message returned on mismatched aspect ratio"), !Err.IsEmpty());
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
