#include "UI/GV2ScreenRegistry.h"

#include "Bridge/GV2StableIdUE.h"
#include "UI/GV2GameShellWidgetBase.h"
#include "UI/GV2ScreenWidgetBase.h"

const FName UGV2ScreenRegistry::LayerEmbedded = TEXT("embedded");

bool UGV2ScreenRegistry::IsValidLayer(FName Layer)
{
    return Layer == LayerEmbedded || UGV2GameShellWidgetBase::IsValidLayerName(Layer);
}

bool UGV2ScreenRegistry::IsLayerAllowedForTopLevel(FName Layer)
{
    return Layer != LayerEmbedded && UGV2GameShellWidgetBase::IsValidLayerName(Layer);
}

bool UGV2ScreenRegistry::IsLayerAllowedForEmbedded(FName Layer)
{
    return Layer == LayerEmbedded;
}

bool UGV2ScreenRegistry::IsAssetAllowedForScreenNamespace(const FString& ScreenNamespace, const FString& AssetPath)
{
    const FString LowerPath = AssetPath.ToLower();
    const FString LowerNamespace = ScreenNamespace.ToLower();

    if (LowerNamespace == TEXT("core"))
    {
        // Core cannot reference textsystem, rh, or higher mod package directories
        if (LowerPath.StartsWith(TEXT("/game/textsystem/"))
            || LowerPath.StartsWith(TEXT("/game/rh/")))
        {
            return false;
        }
        return true;
    }
    else if (LowerNamespace == TEXT("textsystem"))
    {
        // TextSystem cannot reference rh or higher mod directories
        if (LowerPath.StartsWith(TEXT("/game/rh/")))
        {
            return false;
        }
        return true;
    }

    // rh or other packages can reference their own layer or lower layers (textsystem, core/ui)
    return true;
}

const FGV2ScreenRegistryEntry* UGV2ScreenRegistry::FindEntry(const FString& ScreenId) const
{
    return Entries.FindByPredicate([&ScreenId](const FGV2ScreenRegistryEntry& Entry)
    {
        return Entry.ScreenId == ScreenId;
    });
}

bool UGV2ScreenRegistry::Validate(FString& OutError) const
{
    if (Entries.IsEmpty())
    {
        OutError = TEXT("Screen Registry contains no entries");
        return false;
    }

    TSet<FString> SeenScreenIds;
    for (const FGV2ScreenRegistryEntry& Entry : Entries)
    {
        if (!GV2StableIdUE::IsOfKind(Entry.ScreenId, "screen"))
        {
            OutError = FString::Printf(TEXT("Invalid screen_id: '%s'"), *Entry.ScreenId);
            return false;
        }

        if (SeenScreenIds.Contains(Entry.ScreenId))
        {
            OutError = FString::Printf(TEXT("Duplicate screen_id: '%s'"), *Entry.ScreenId);
            return false;
        }
        SeenScreenIds.Add(Entry.ScreenId);

        if (!IsValidLayer(Entry.Layer))
        {
            OutError = FString::Printf(TEXT("Unknown layer '%s' for screen_id '%s'"), *Entry.Layer.ToString(), *Entry.ScreenId);
            return false;
        }

        if (Entry.WidgetClass.IsNull())
        {
            OutError = FString::Printf(TEXT("Null WidgetClass for screen_id '%s'"), *Entry.ScreenId);
            return false;
        }

        int32 ColonIdx = INDEX_NONE;
        if (Entry.ScreenId.FindChar(TEXT(':'), ColonIdx))
        {
            const FString Namespace = Entry.ScreenId.Left(ColonIdx);
            const FString AssetPath = Entry.WidgetClass.ToSoftObjectPath().ToString();
            if (!IsAssetAllowedForScreenNamespace(Namespace, AssetPath))
            {
                OutError = FString::Printf(
                    TEXT("Screen '%s' in namespace '%s' violates layer ownership by referencing higher layer asset '%s'"),
                    *Entry.ScreenId,
                    *Namespace,
                    *AssetPath);
                return false;
            }
        }
    }

    return true;
}
