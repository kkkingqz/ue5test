#pragma once

#include "GV2ContentCore/GV2ContentCore.h"
#include "GV2ContentCore/Value.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace GV2ContentCore
{
/** Incremental streaming SHA-256 hash builder. */
class GV2_CONTENT_CORE_API FSha256Builder final
{
public:
    FSha256Builder();
    void Update(const void* Data, std::size_t Length);
    void Update(std::string_view Text);
    std::string FinalizeHex();

private:
    void ProcessBlock(const std::uint8_t* Block);

    std::uint64_t TotalBytes = 0;
    std::array<std::uint32_t, 8> State{};
    std::array<std::uint8_t, 64> Buffer{};
    std::size_t BufferLen = 0;
    bool bFinalized = false;
};

/** Computes SHA-256 hash of arbitrary binary or text data. */
GV2_CONTENT_CORE_API std::string ComputeSha256(std::string_view Data);

/** Computes SHA-256 canonical hash of the structured Value. */
GV2_CONTENT_CORE_API std::string ComputeCanonicalHash(const FValue& Value);

/** Returns true if Text contains exactly 64 lowercase ASCII hex characters [0-9a-f]. */
GV2_CONTENT_CORE_API bool IsCanonicalSha256(std::string_view Text) noexcept;
}

