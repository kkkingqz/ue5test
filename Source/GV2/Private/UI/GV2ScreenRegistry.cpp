#include "UI/GV2ScreenRegistry.h"

#include "Bridge/GV2StableIdUE.h"
#include "UI/GV2GameShellWidgetBase.h"
#include "UI/GV2ScreenWidgetBase.h"

bool UGV2ScreenRegistry::IsValidLayer(FName Layer)
{
    return UGV2GameShellWidgetBase::IsValidLayerName(Layer);
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
    }

    return true;
}
