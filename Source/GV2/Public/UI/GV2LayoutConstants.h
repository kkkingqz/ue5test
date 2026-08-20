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
};
