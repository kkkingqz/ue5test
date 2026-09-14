#pragma once

#include "GV2RuntimeCore/GV2RuntimeSession.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace GV2RuntimeCore
{
enum class EResourceKind : std::uint8_t
{
    Unknown,
    Image,
    Audio,
    Video,
    WidgetFactory
};

struct FResourceMetadata
{
    std::string ResourceId;
    EResourceKind Kind = EResourceKind::Unknown;
    bool bAvailable = false;
};

class GV2_PORTABLE_API IResourceCatalog
{
public:
    virtual ~IResourceCatalog() = default;
    virtual std::optional<FResourceMetadata> FindMetadata(const std::string& ResourceId) const = 0;
};

class GV2_PORTABLE_API ILocalizationAdapter
{
public:
    virtual ~ILocalizationAdapter() = default;

    // Returns empty when the host intentionally preserves unresolved TextSpec.
    virtual std::optional<std::string> Resolve(
        const FTextSpec& Text,
        const std::string& Locale) const = 0;
};

// SAV-05 (plan SaveAndLoad, ADR-0021): slot-scoped opaque byte storage. Lua
// addresses a slot only by SlotId; the interface never carries a path,
// FString, UObject, or filesystem type, so nothing above this boundary can
// depend on how (or whether) a slot maps to a physical file. The absence of
// any concrete storage does not prevent a session from starting without
// saving — this interface is never required by FRuntimeSession::Start,
// matching IResourceCatalog/ILocalizationAdapter above.
enum class ESaveSlotRevision : std::uint8_t
{
    Current,
    Previous,
};

enum class ESaveSlotResult : std::uint8_t
{
    Ok,
    NotFound,
    Unreadable,
    Failure,
    Busy,
};

struct FSaveSlotReadResult
{
    ESaveSlotResult Result = ESaveSlotResult::NotFound;
    // Opaque bytes; only meaningful when Result == Ok. May contain any byte
    // value, including embedded NUL — never treated as text.
    std::string Bytes;
};

struct FSaveSlotWriteResult
{
    ESaveSlotResult Result = ESaveSlotResult::Failure;
};

class GV2_PORTABLE_API ISaveSlotStorage
{
public:
    virtual ~ISaveSlotStorage() = default;

    virtual FSaveSlotReadResult ReadSlot(
        const std::string& SlotId,
        ESaveSlotRevision Revision = ESaveSlotRevision::Current) const = 0;

    // Atomic replace: when Result == Ok the slot now holds exactly Bytes;
    // for any other result the slot's previous content (if any) is left
    // valid and readable — a failed write never leaves a partially-written
    // slot in its place.
    virtual FSaveSlotWriteResult WriteSlot(const std::string& SlotId, const std::string& Bytes) = 0;
};

// True only for slot ids safe to turn into a single path segment: non-empty
// and matching the same segment grammar as module_id/instance_id/spec case
// names (^[a-z][a-z0-9_]*$), so a slot id can never contain a path
// separator or a ".." traversal component.
GV2_PORTABLE_API bool IsValidSaveSlotId(const std::string& SlotId);

class FFilesystemSaveSlotStorage;

// CFC-08: factory Open result. Storage is a unique owner owning an exclusive
// process-level lock on RootDir; null on Busy or Failure.
struct FSaveSlotStorageOpenResult
{
    ESaveSlotResult Result = ESaveSlotResult::Failure;
    std::unique_ptr<FFilesystemSaveSlotStorage> Storage;
};

// SAV-06 / CFC-08: the one filesystem-backed ISaveSlotStorage implementation, shared
// verbatim by both hosts — each supplies its own RootDir at construction via Open()
// (std::filesystem::path is already an accepted portable type at this
// layer, see GV2ContentHostSupport's discovery headers). Resolving SlotId
// to a path, and confining that path inside RootDir, is entirely internal:
// nothing above ISaveSlotStorage ever sees a path. Writes go to a temporary
// file inside RootDir, flush/close, rename to immutable generation, write
// head (new_generation, old_current), and atomic rename head on the same volume —
// a write that fails at any point before the head rename leaves the
// previously-published slot generation(s) untouched.
namespace Testing { class FGV2SaveSlotStorageTestAccess; }

class GV2_PORTABLE_API FFilesystemSaveSlotStorage final : public ISaveSlotStorage
{
public:
    ~FFilesystemSaveSlotStorage() override;
    FFilesystemSaveSlotStorage(const FFilesystemSaveSlotStorage&) = delete;
    FFilesystemSaveSlotStorage& operator=(const FFilesystemSaveSlotStorage&) = delete;
    FFilesystemSaveSlotStorage(FFilesystemSaveSlotStorage&&) = delete;
    FFilesystemSaveSlotStorage& operator=(FFilesystemSaveSlotStorage&&) = delete;

    static FSaveSlotStorageOpenResult Open(const std::filesystem::path& InRootDir);

    FSaveSlotReadResult ReadSlot(
        const std::string& SlotId,
        ESaveSlotRevision Revision = ESaveSlotRevision::Current) const override;
    FSaveSlotWriteResult WriteSlot(const std::string& SlotId, const std::string& Bytes) override;

    const std::filesystem::path& GetRootDir() const;

private:
    friend class Testing::FGV2SaveSlotStorageTestAccess;

    struct FImpl;
    explicit FFilesystemSaveSlotStorage(std::unique_ptr<FImpl> InImpl);

    std::unique_ptr<FImpl> Impl;
};
}
