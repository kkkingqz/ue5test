#pragma once

#include "CoreMinimal.h"

/**
 * FGV2LayoutConstants (UIF-06, ADR-0035)
 * Defines the dual-resolution references for raster authoring vs layout calculation:
 * - Raster authoring is anchored at 3840 x 2160 (4K) for crisp asset rendering and clean downsampling.
 * - Virtual layout coordinates are expressed in 1920 x 1080 (Full HD) units.
 * - RasterToLayoutScale is 2.0.
 * - MinSupportedViewport is 1280 x 720 (minimum supported target).
 */
struct GV2_API FGV2LayoutConstants
{
    /** Authoring reference resolution for raster textures and 9-slice assets (4K). */
    static constexpr float RasterAuthoringWidth = 3840.0f;
    static constexpr float RasterAuthoringHeight = 2160.0f;

    /** Canonical reference units for screen templates and layout definitions (1080p). */
    static constexpr float VirtualLayoutWidth = 1920.0f;
    static constexpr float VirtualLayoutHeight = 1080.0f;

    /** Ratio of raster authoring resolution to layout units (3840 / 1920 = 2.0). */
    static constexpr float RasterToLayoutScale = 2.0f;

    /** Minimum supported viewport resolution (720p). */
    static constexpr float MinSupportedViewportWidth = 1280.0f;
    static constexpr float MinSupportedViewportHeight = 720.0f;

    /** Reference aspect ratio (16:9 = 1.7777778). */
    static constexpr float StandardAspectRatio = 16.0f / 9.0f;

    /** Ultrawide 21:9 reference aspect ratio (21:9 = 2.3333333). */
    static constexpr float UltrawideAspectRatio = 21.0f / 9.0f;

    /**
     * Validates relational invariants between layout values.
     * Returns true if all proportional and relational constraints are satisfied.
     */
    static bool ValidateInvariantsValues(
        float InRasterWidth,
        float InRasterHeight,
        float InVirtualWidth,
        float InVirtualHeight,
        float InScale,
        float InMinWidth,
        float InMinHeight,
        float InStdAspect,
        float InUltrawideAspect,
        FString* OutError = nullptr)
    {
        if (InRasterWidth <= 0.0f || InRasterHeight <= 0.0f ||
            InVirtualWidth <= 0.0f || InVirtualHeight <= 0.0f ||
            InScale <= 0.0f || InMinWidth <= 0.0f || InMinHeight <= 0.0f ||
            InStdAspect <= 0.0f || InUltrawideAspect <= 0.0f)
        {
            if (OutError) *OutError = TEXT("Dimensions, scale, and aspect ratios must be strictly positive");
            return false;
        }

        const float ComputedWidthScale = InRasterWidth / InVirtualWidth;
        if (!FMath::IsNearlyEqual(ComputedWidthScale, InScale, 1e-4f))
        {
            if (OutError) *OutError = FString::Printf(TEXT("RasterToLayoutScale (%f) does not match width ratio (%f / %f = %f)"),
                InScale, InRasterWidth, InVirtualWidth, ComputedWidthScale);
            return false;
        }

        const float ComputedHeightScale = InRasterHeight / InVirtualHeight;
        if (!FMath::IsNearlyEqual(ComputedHeightScale, InScale, 1e-4f))
        {
            if (OutError) *OutError = FString::Printf(TEXT("RasterToLayoutScale (%f) does not match height ratio (%f / %f = %f)"),
                InScale, InRasterHeight, InVirtualHeight, ComputedHeightScale);
            return false;
        }

        const float VirtualAspect = InVirtualWidth / InVirtualHeight;
        if (!FMath::IsNearlyEqual(VirtualAspect, InStdAspect, 1e-4f))
        {
            if (OutError) *OutError = FString::Printf(TEXT("Virtual layout aspect ratio (%f / %f = %f) does not match StandardAspectRatio (%f)"),
                InVirtualWidth, InVirtualHeight, VirtualAspect, InStdAspect);
            return false;
        }

        const float RasterAspect = InRasterWidth / InRasterHeight;
        if (!FMath::IsNearlyEqual(RasterAspect, InStdAspect, 1e-4f))
        {
            if (OutError) *OutError = FString::Printf(TEXT("Raster authoring aspect ratio (%f / %f = %f) does not match StandardAspectRatio (%f)"),
                InRasterWidth, InRasterHeight, RasterAspect, InStdAspect);
            return false;
        }

        const float MinViewportAspect = InMinWidth / InMinHeight;
        if (!FMath::IsNearlyEqual(MinViewportAspect, InStdAspect, 1e-4f))
        {
            if (OutError) *OutError = FString::Printf(TEXT("MinSupportedViewport aspect ratio (%f / %f = %f) does not match StandardAspectRatio (%f)"),
                InMinWidth, InMinHeight, MinViewportAspect, InStdAspect);
            return false;
        }

        if (InMinWidth > InVirtualWidth || InMinHeight > InVirtualHeight)
        {
            if (OutError) *OutError = FString::Printf(TEXT("MinSupportedViewport (%f x %f) exceeds VirtualLayout dimensions (%f x %f)"),
                InMinWidth, InMinHeight, InVirtualWidth, InVirtualHeight);
            return false;
        }

        if (InUltrawideAspect <= InStdAspect)
        {
            if (OutError) *OutError = FString::Printf(TEXT("UltrawideAspectRatio (%f) must be strictly greater than StandardAspectRatio (%f)"),
                InUltrawideAspect, InStdAspect);
            return false;
        }

        return true;
    }

    /** Validates relational invariants for the active constant values. */
    static bool ValidateInvariants(FString* OutError = nullptr)
    {
        return ValidateInvariantsValues(
            RasterAuthoringWidth,
            RasterAuthoringHeight,
            VirtualLayoutWidth,
            VirtualLayoutHeight,
            RasterToLayoutScale,
            MinSupportedViewportWidth,
            MinSupportedViewportHeight,
            StandardAspectRatio,
            UltrawideAspectRatio,
            OutError);
    }
};
