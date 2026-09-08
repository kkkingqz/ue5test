#include "UI/GV2ImageResourceCatalog.h"
#include "UI/GV2PresentationAuthorityProbe.h"

#include "Bridge/GV2StableIdUE.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
bool IsCanonicalResourceId(const FString& ResourceId)
{
    return GV2StableIdUE::IsOfKind(ResourceId, "resource");
}

bool IsFinitePositive(const float Value)
{
    return FMath::IsFinite(Value) && Value > 0.0f;
}

enum class ESourceImageKind : uint8
{
    FixedAspect,
    NineSlice,
    Tile
};

ESourceImageKind GetSourceImageKind(const FString& Filename)
{
    if (Filename.EndsWith(TEXT(".9.png"), ESearchCase::CaseSensitive))
    {
        return ESourceImageKind::NineSlice;
    }
    if (Filename.EndsWith(TEXT(".tile.png"), ESearchCase::CaseSensitive))
    {
        return ESourceImageKind::Tile;
    }
    return ESourceImageKind::FixedAspect;
}

FString GetCanonicalSourceBasename(const FString& Filename)
{
    FString Basename = FPaths::GetBaseFilename(Filename);
    if (Basename.EndsWith(TEXT(".9"), ESearchCase::CaseSensitive))
    {
        Basename.LeftChopInline(2);
    }
    else if (Basename.EndsWith(TEXT(".tile"), ESearchCase::CaseSensitive))
    {
        Basename.LeftChopInline(5);
    }
    return Basename;
}

bool IsNineSliceMarker(const FColor& Pixel)
{
    return Pixel.A >= 128 && Pixel.R <= 32 && Pixel.G <= 32 && Pixel.B <= 32;
}

bool FindSingleMarkerRun(
    const TFunctionRef<const FColor&(int32)> GetPixel,
    const int32 Count,
    int32& OutStart,
    int32& OutEndExclusive)
{
    OutStart = INDEX_NONE;
    OutEndExclusive = INDEX_NONE;
    bool bRunEnded = false;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        if (IsNineSliceMarker(GetPixel(Index)))
        {
            if (bRunEnded)
            {
                return false;
            }
            if (OutStart == INDEX_NONE)
            {
                OutStart = Index;
            }
            OutEndExclusive = Index + 1;
        }
        else if (OutStart != INDEX_NONE)
        {
            bRunEnded = true;
        }
    }
    return OutStart != INDEX_NONE;
}

bool DecodeNineSliceImage(FImage& Image, FMargin& OutBorders, FString& OutError)
{
    if (Image.SizeX < 3 || Image.SizeY < 3)
    {
        OutError = TEXT("Nine-slice PNG must include a one-pixel marker border and a non-empty body.");
        return false;
    }

    const int32 SourceWidth = Image.SizeX;
    const int32 SourceHeight = Image.SizeY;
    const int32 InnerWidth = SourceWidth - 2;
    const int32 InnerHeight = SourceHeight - 2;
    const FColor* SourcePixels = reinterpret_cast<const FColor*>(Image.RawData.GetData());
    int32 HorizontalStart = INDEX_NONE;
    int32 HorizontalEnd = INDEX_NONE;
    int32 VerticalStart = INDEX_NONE;
    int32 VerticalEnd = INDEX_NONE;
    if (!FindSingleMarkerRun(
            [&SourcePixels, SourceWidth](const int32 Index) -> const FColor&
            {
                return SourcePixels[Index + 1];
            },
            InnerWidth,
            HorizontalStart,
            HorizontalEnd)
        || !FindSingleMarkerRun(
            [&SourcePixels, SourceWidth](const int32 Index) -> const FColor&
            {
                return SourcePixels[(Index + 1) * SourceWidth];
            },
            InnerHeight,
            VerticalStart,
            VerticalEnd))
    {
        OutError = TEXT("Nine-slice PNG requires exactly one black marker run on its top and left border.");
        return false;
    }

    OutBorders = FMargin(
        HorizontalStart,
        VerticalStart,
        InnerWidth - HorizontalEnd,
        InnerHeight - VerticalEnd);
    FImage InnerImage(InnerWidth, InnerHeight, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
    FColor* InnerPixels = reinterpret_cast<FColor*>(InnerImage.RawData.GetData());
    for (int32 Y = 0; Y < InnerHeight; ++Y)
    {
        FMemory::Memcpy(
            InnerPixels + Y * InnerWidth,
            SourcePixels + (Y + 1) * SourceWidth + 1,
            InnerWidth * sizeof(FColor));
    }
    Image = MoveTemp(InnerImage);
    return true;
}

// PAH-04B: session-scoped, not process-lifetime -- see RebuildForSession/
// ReleaseForSession/GetSessionCatalog.
TStrongObjectPtr<UGV2ImageResourceCatalog> GSessionImageCatalog;

FString GetProjectResourcesRoot()
{
    return FPaths::Combine(FPaths::ProjectDir(), TEXT("Resources"));
}

// PSC-07: shared by BuildFromDirectory's generic scan and
// BuildFromPackageResourceRoots' scoped-per-package scan -- both already know the
// resource_id (derived by their own, different grammar checks) before reaching this
// point, so this only owns decode/texture-creation/resolve, not namespace derivation.
bool BuildEntryFromPngFile(
    const FString& PngFile,
    const FString& ResourceId,
    FGV2ImageResourceDefinition& OutDefinition,
    FGV2ResolvedImageResource& OutResolved,
    FString& OutError)
{
    FImage SourceImage;
    if (!FImageUtils::LoadImage(*PngFile, SourceImage)
        || SourceImage.SizeX <= 0 || SourceImage.SizeY <= 0)
    {
        OutError = FString::Printf(TEXT("Cannot decode PNG resource: %s"), *PngFile);
        return false;
    }
    SourceImage.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);

    FGV2ImageResourceDefinition Definition;
    Definition.ResourceId = ResourceId;
    switch (GetSourceImageKind(PngFile))
    {
    case ESourceImageKind::FixedAspect:
        Definition.RenderMode = EGV2ImageRenderMode::FixedAspect;
        Definition.FixedAspectRatio = static_cast<float>(SourceImage.SizeX)
            / static_cast<float>(SourceImage.SizeY);
        break;
    case ESourceImageKind::NineSlice:
        Definition.RenderMode = EGV2ImageRenderMode::NineSlice;
        if (!DecodeNineSliceImage(SourceImage, Definition.NineSliceBorderPixels, OutError))
        {
            OutError = FString::Printf(TEXT("%s: %s"), *PngFile, *OutError);
            return false;
        }
        break;
    case ESourceImageKind::Tile:
        Definition.RenderMode = EGV2ImageRenderMode::Tile;
        Definition.TileSize = FVector2D(SourceImage.SizeX, SourceImage.SizeY);
        break;
    }

    UTexture2D* Texture = FImageUtils::CreateTexture2DFromImage(SourceImage);
    if (Texture == nullptr)
    {
        OutError = FString::Printf(TEXT("Cannot create runtime texture: %s"), *PngFile);
        return false;
    }
    Texture->NeverStream = true;
    Definition.Texture = Texture;

    if (!UGV2ImageResourceCatalog::ResolveDefinition(Definition, OutResolved, OutError))
    {
        OutError = FString::Printf(TEXT("%s: %s"), *PngFile, *OutError);
        return false;
    }

    OutDefinition = MoveTemp(Definition);
    return true;
}
}

bool UGV2ImageResourceCatalog::TryMakeResourceId(
    const FString& RootDirectory,
    const FString& PngFilename,
    FString& OutResourceId,
    FString& OutError)
{
    OutResourceId.Reset();
    FString Root = FPaths::ConvertRelativePathToFull(RootDirectory);
    FString Filename = FPaths::ConvertRelativePathToFull(PngFilename);
    FPaths::NormalizeDirectoryName(Root);
    FPaths::NormalizeFilename(Filename);
    const FString Prefix = Root + TEXT("/");
    if (!Filename.StartsWith(Prefix, ESearchCase::CaseSensitive)
        || FPaths::GetExtension(Filename, true) != TEXT(".png"))
    {
        OutError = TEXT("Image file must be a lowercase .png below the configured resource root.");
        return false;
    }

    FString Relative = Filename.RightChop(Prefix.Len());
    TArray<FString> Segments;
    Relative.ParseIntoArray(Segments, TEXT("/"), false);
    if (Segments.Num() < 4 || Segments[1] != TEXT("resource"))
    {
        OutError = TEXT("Image path must be <namespace>/resource/<path>.png.");
        return false;
    }
    Segments.Last() = GetCanonicalSourceBasename(Segments.Last());
    const FString Namespace = Segments[0];
    Segments.RemoveAt(0, 2);
    OutResourceId = Namespace + TEXT(":resource.") + FString::Join(Segments, TEXT("."));
    if (!IsCanonicalResourceId(OutResourceId))
    {
        OutError = TEXT("Image path contains a non-canonical Stable ID segment.");
        OutResourceId.Reset();
        return false;
    }
    OutError.Reset();
    return true;
}

bool UGV2ImageResourceCatalog::TryMakeResourceIdForPackage(
    const FString& PackageId,
    const FString& PackageResourceRoot,
    const FString& PngFilename,
    FString& OutResourceId,
    FString& OutError)
{
    OutResourceId.Reset();
    FString Root = FPaths::ConvertRelativePathToFull(PackageResourceRoot);
    FString Filename = FPaths::ConvertRelativePathToFull(PngFilename);
    FPaths::NormalizeDirectoryName(Root);
    FPaths::NormalizeFilename(Filename);
    const FString Prefix = Root + TEXT("/");
    if (!Filename.StartsWith(Prefix, ESearchCase::CaseSensitive)
        || FPaths::GetExtension(Filename, true) != TEXT(".png"))
    {
        OutError = TEXT("Image file must be a lowercase .png below the package's own resource root.");
        return false;
    }

    FString Relative = Filename.RightChop(Prefix.Len());
    TArray<FString> Segments;
    Relative.ParseIntoArray(Segments, TEXT("/"), false);
    if (Segments.Num() < 3 || Segments[0] != TEXT("resource"))
    {
        OutError = TEXT("Image path must be resource/<path>.png below the package's own resource root.");
        return false;
    }
    Segments.Last() = GetCanonicalSourceBasename(Segments.Last());
    Segments.RemoveAt(0);
    OutResourceId = PackageId + TEXT(":resource.") + FString::Join(Segments, TEXT("."));
    if (!IsCanonicalResourceId(OutResourceId))
    {
        OutError = TEXT("Image path contains a non-canonical Stable ID segment.");
        OutResourceId.Reset();
        return false;
    }

    // PSC-07: secondary defense (see this function's header comment) -- re-derive the
    // namespace segment from the id we just produced and check it matches the PackageId
    // whose own root we were told to scan.
    int32 ColonIndex = INDEX_NONE;
    if (!OutResourceId.FindChar(TEXT(':'), ColonIndex) || OutResourceId.Left(ColonIndex) != PackageId)
    {
        OutError = TEXT("Image resource namespace does not match its owning package.");
        OutResourceId.Reset();
        return false;
    }
    OutError.Reset();
    return true;
}

// PAH-04: pre_ready_discovery -- a generic, unscoped directory scan. PSC-07 stopped
// routing production session bootstrap through this function (BuildFromPackageClosure
// now calls the scoped BuildFromPackageResourceRoots() below instead, never this one) --
// its only remaining callers are direct scanner tests (excluded from this gate) that
// exercise the raw scan/decode grammar in isolation from package-closure scoping.
bool UGV2ImageResourceCatalog::BuildFromDirectory(
    const FString& RootDirectory,
    FString& OutError)
{
    TArray<FString> PngFiles;
    IFileManager::Get().FindFilesRecursive(
        PngFiles,
        *RootDirectory,
        TEXT("*.png"),
        true,
        false,
        false);
    PngFiles.Sort();

    TArray<FGV2ImageResourceDefinition> CandidateEntries;
    TMap<FString, FGV2ResolvedImageResource> CandidateResolvedById;
    TArray<TObjectPtr<UTexture2D>> CandidateTextures;
    TSet<FString> SeenIds;
    for (const FString& PngFile : PngFiles)
    {
        FString ResourceId;
        if (!TryMakeResourceId(RootDirectory, PngFile, ResourceId, OutError))
        {
            return false;
        }
        if (SeenIds.Contains(ResourceId))
        {
            OutError = FString::Printf(TEXT("Duplicate image resource_id: %s"), *ResourceId);
            return false;
        }

        FGV2ImageResourceDefinition Definition;
        FGV2ResolvedImageResource ResolvedResource;
        if (!BuildEntryFromPngFile(PngFile, ResourceId, Definition, ResolvedResource, OutError))
        {
            return false;
        }
        SeenIds.Add(ResourceId);
        CandidateResolvedById.Add(ResourceId, MoveTemp(ResolvedResource));
        CandidateTextures.Add(Definition.Texture.Get());
        CandidateEntries.Add(MoveTemp(Definition));
    }

    Entries = MoveTemp(CandidateEntries);
    ResolvedById = MoveTemp(CandidateResolvedById);
    RuntimeTextures = MoveTemp(CandidateTextures);
    OutError.Reset();
    return true;
}

bool UGV2ImageResourceCatalog::ValidateDefinition(
    const FGV2ImageResourceDefinition& Definition,
    FString& OutError)
{
    OutError.Reset();
    if (!IsCanonicalResourceId(Definition.ResourceId))
    {
        OutError = TEXT("Image resource_id must be a canonical Stable ID of kind resource.");
        return false;
    }
    if (Definition.Texture.IsNull())
    {
        OutError = TEXT("Image resource texture is required.");
        return false;
    }

    switch (Definition.RenderMode)
    {
    case EGV2ImageRenderMode::FixedAspect:
        if (!IsFinitePositive(Definition.FixedAspectRatio))
        {
            OutError = TEXT("fixed_aspect resource requires a positive finite aspect ratio.");
            return false;
        }
        break;
    case EGV2ImageRenderMode::NineSlice:
        if (!FMath::IsFinite(static_cast<float>(Definition.NineSliceBorderPixels.Left))
            || !FMath::IsFinite(static_cast<float>(Definition.NineSliceBorderPixels.Top))
            || !FMath::IsFinite(static_cast<float>(Definition.NineSliceBorderPixels.Right))
            || !FMath::IsFinite(static_cast<float>(Definition.NineSliceBorderPixels.Bottom))
            || Definition.NineSliceBorderPixels.Left < 0.0f
            || Definition.NineSliceBorderPixels.Top < 0.0f
            || Definition.NineSliceBorderPixels.Right < 0.0f
            || Definition.NineSliceBorderPixels.Bottom < 0.0f)
        {
            OutError = TEXT("nine_slice borders cannot be negative.");
            return false;
        }
        break;
    case EGV2ImageRenderMode::Tile:
        if (!IsFinitePositive(Definition.TileSize.X) || !IsFinitePositive(Definition.TileSize.Y))
        {
            OutError = TEXT("tile resource requires a positive finite logical tile size.");
            return false;
        }
        break;
    default:
        OutError = TEXT("Image resource render mode is unknown.");
        return false;
    }
    return true;
}

bool UGV2ImageResourceCatalog::ResolveDefinition(
    const FGV2ImageResourceDefinition& Definition,
    FGV2ResolvedImageResource& OutResource,
    FString& OutError)
{
    OutResource = {};
    if (!ValidateDefinition(Definition, OutError))
    {
        return false;
    }

    UTexture2D* Texture = Definition.Texture.LoadSynchronous();
    if (Texture == nullptr)
    {
        OutError = TEXT("Image resource texture could not be loaded.");
        return false;
    }

    const float TextureWidth = static_cast<float>(Texture->GetSizeX());
    const float TextureHeight = static_cast<float>(Texture->GetSizeY());
    if (!IsFinitePositive(TextureWidth) || !IsFinitePositive(TextureHeight))
    {
        OutError = TEXT("Image resource texture has invalid dimensions.");
        return false;
    }

    FSlateBrush Brush;
    Brush.SetResourceObject(Texture);
    Brush.ImageSize = FVector2D(TextureWidth, TextureHeight);
    Brush.DrawAs = ESlateBrushDrawType::Image;
    Brush.Tiling = ESlateBrushTileType::NoTile;
    Brush.ImageType = ESlateBrushImageType::FullColor;

    switch (Definition.RenderMode)
    {
    case EGV2ImageRenderMode::FixedAspect:
        Brush.ImageSize = Definition.FixedAspectRatio >= 1.0f
            ? FVector2D(100.0f * Definition.FixedAspectRatio, 100.0f)
            : FVector2D(100.0f, 100.0f / Definition.FixedAspectRatio);
        break;
    case EGV2ImageRenderMode::NineSlice:
        if (Definition.NineSliceBorderPixels.Left + Definition.NineSliceBorderPixels.Right >= TextureWidth
            || Definition.NineSliceBorderPixels.Top + Definition.NineSliceBorderPixels.Bottom >= TextureHeight)
        {
            OutError = TEXT("nine_slice borders must leave a positive center region.");
            return false;
        }
        Brush.DrawAs = ESlateBrushDrawType::Box;
        Brush.Margin = FMargin(
            Definition.NineSliceBorderPixels.Left / TextureWidth,
            Definition.NineSliceBorderPixels.Top / TextureHeight,
            Definition.NineSliceBorderPixels.Right / TextureWidth,
            Definition.NineSliceBorderPixels.Bottom / TextureHeight);
        break;
    case EGV2ImageRenderMode::Tile:
        Brush.ImageSize = Definition.TileSize;
        Brush.Tiling = ESlateBrushTileType::Both;
        break;
    default:
        OutError = TEXT("Image resource render mode is unknown.");
        return false;
    }

    OutResource.ResourceId = Definition.ResourceId;
    OutResource.RenderMode = Definition.RenderMode;
    OutResource.FixedAspectRatio = Definition.RenderMode == EGV2ImageRenderMode::FixedAspect
        ? Definition.FixedAspectRatio
        : 0.0f;
    OutResource.Brush = MoveTemp(Brush);
    return true;
}

bool UGV2ImageResourceCatalog::Validate(FString& OutError) const
{
    TSet<FString> SeenIds;
    for (const FGV2ImageResourceDefinition& Definition : Entries)
    {
        if (!ValidateDefinition(Definition, OutError))
        {
            return false;
        }
        if (SeenIds.Contains(Definition.ResourceId))
        {
            OutError = FString::Printf(TEXT("Duplicate image resource_id: %s"), *Definition.ResourceId);
            return false;
        }
        SeenIds.Add(Definition.ResourceId);
    }
    return true;
}

// PAH-08: phase=authority
bool UGV2ImageResourceCatalog::Resolve(
    const FString& ResourceId,
    FGV2ResolvedImageResource& OutResource,
    FString& OutError) const
{
    GV2_NOTE_AUTHORITY_RESOLVE();
    OutResource = {};
    if (!IsCanonicalResourceId(ResourceId))
    {
        OutError = TEXT("Requested image resource_id is not canonical.");
        return false;
    }
    const FGV2ResolvedImageResource* Resolved = ResolvedById.Find(ResourceId);
    if (Resolved == nullptr)
    {
        OutError = FString::Printf(TEXT("Unknown image resource_id: %s"), *ResourceId);
        return false;
    }
    OutResource = *Resolved;
    OutError.Reset();
    return true;
}

// PAH-04: pre_ready_discovery -- only called from BuildFromPackageClosure(), only
// called from RebuildForSession(), only called from StartSession()/
// FGV2SessionContentCandidate::Build() (PAH-04B/PSC-07: the sole production discovery
// path for image resources; never reached after a session reaches Ready).
bool UGV2ImageResourceCatalog::BuildFromPackageResourceRoots(
    const TArray<FGV2ImagePackageResourceRoot>& PackageResourceRoots,
    FString& OutError)
{
    TArray<FGV2ImageResourceDefinition> CandidateEntries;
    TMap<FString, FGV2ResolvedImageResource> CandidateResolvedById;
    TArray<TObjectPtr<UTexture2D>> CandidateTextures;
    TSet<FString> SeenIds;

    for (const FGV2ImagePackageResourceRoot& PackageRoot : PackageResourceRoots)
    {
        // PSC-07 (PAH-R5): recursing into exactly this one package's own root is what
        // makes a sibling (disabled) package's directory physically unreachable here --
        // not a filter applied to a wholesale scan's results afterward.
        TArray<FString> PngFiles;
        IFileManager::Get().FindFilesRecursive(
            PngFiles,
            *PackageRoot.ResourceRoot,
            TEXT("*.png"),
            true,
            false,
            false);
        PngFiles.Sort();

        for (const FString& PngFile : PngFiles)
        {
            FString ResourceId;
            if (!TryMakeResourceIdForPackage(PackageRoot.PackageId, PackageRoot.ResourceRoot, PngFile, ResourceId, OutError))
            {
                return false;
            }
            if (SeenIds.Contains(ResourceId))
            {
                OutError = FString::Printf(TEXT("Duplicate image resource_id: %s"), *ResourceId);
                return false;
            }

            FGV2ImageResourceDefinition Definition;
            FGV2ResolvedImageResource ResolvedResource;
            if (!BuildEntryFromPngFile(PngFile, ResourceId, Definition, ResolvedResource, OutError))
            {
                return false;
            }
            SeenIds.Add(ResourceId);
            CandidateResolvedById.Add(ResourceId, MoveTemp(ResolvedResource));
            CandidateTextures.Add(Definition.Texture.Get());
            CandidateEntries.Add(MoveTemp(Definition));
        }
    }

    Entries = MoveTemp(CandidateEntries);
    ResolvedById = MoveTemp(CandidateResolvedById);
    RuntimeTextures = MoveTemp(CandidateTextures);
    OutError.Reset();
    return true;
}

bool UGV2ImageResourceCatalog::BuildFromPackageClosure(const TArray<FString>& PackageIds, FString& OutError)
{
    const FString ResourcesRoot = GetProjectResourcesRoot();
    TArray<FGV2ImagePackageResourceRoot> PackageResourceRoots;
    PackageResourceRoots.Reserve(PackageIds.Num());
    for (const FString& PackageId : PackageIds)
    {
        PackageResourceRoots.Add(FGV2ImagePackageResourceRoot{PackageId, FPaths::Combine(ResourcesRoot, PackageId)});
    }
    return BuildFromPackageResourceRoots(PackageResourceRoots, OutError);
}

bool UGV2ImageResourceCatalog::RebuildForSession(const TArray<FString>& PackageIds, FString& OutError)
{
    TStrongObjectPtr<UGV2ImageResourceCatalog> Candidate(
        NewObject<UGV2ImageResourceCatalog>(GetTransientPackage()));
    if (!Candidate->BuildFromPackageClosure(PackageIds, OutError))
    {
        return false;
    }
    GSessionImageCatalog = MoveTemp(Candidate);
    return true;
}

void UGV2ImageResourceCatalog::ReleaseForSession()
{
    GSessionImageCatalog.Reset();
}

UGV2ImageResourceCatalog* UGV2ImageResourceCatalog::GetSessionCatalog()
{
    return GSessionImageCatalog.Get();
}
