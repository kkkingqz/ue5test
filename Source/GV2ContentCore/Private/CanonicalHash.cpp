#include "CanonicalHash.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace GV2ContentCore
{
namespace
{
void AppendLengthPrefixed(std::string& Output, const std::string_view Value)
{
    Output += std::to_string(Value.size());
    Output.push_back(':');
    Output.append(Value);
}

void AppendCanonical(std::string& Output, const FValue& Value)
{
    switch (Value.GetKind())
    {
    case EValueKind::Null:
        Output.push_back('n');
        return;
    case EValueKind::Boolean:
        Output += Value.AsBoolean() ? "b1" : "b0";
        return;
    case EValueKind::Integer:
        Output.push_back('i');
        Output += std::to_string(Value.AsInteger());
        Output.push_back(';');
        return;
    case EValueKind::Number:
    {
        Output.push_back('d');
        const std::uint64_t Bits = std::bit_cast<std::uint64_t>(Value.AsNumber());
        for (int Shift = 60; Shift >= 0; Shift -= 4)
        {
            constexpr char Hex[] = "0123456789abcdef";
            Output.push_back(Hex[(Bits >> Shift) & 0xf]);
        }
        return;
    }
    case EValueKind::String:
        Output.push_back('s');
        AppendLengthPrefixed(Output, Value.AsString());
        return;
    case EValueKind::Array:
        Output.push_back('[');
        Output += std::to_string(Value.AsArray().size());
        Output.push_back(':');
        for (const FValue& Entry : Value.AsArray()) AppendCanonical(Output, Entry);
        Output.push_back(']');
        return;
    case EValueKind::Object:
    {
        Output.push_back('{');
        std::vector<const FValue::FObjectField*> Fields;
        Fields.reserve(Value.AsObject().size());
        for (const auto& Field : Value.AsObject()) Fields.push_back(&Field);
        std::sort(Fields.begin(), Fields.end(), [](const auto* Left, const auto* Right)
        {
            return Left->first < Right->first;
        });
        Output += std::to_string(Fields.size());
        Output.push_back(':');
        for (const auto* Field : Fields)
        {
            AppendLengthPrefixed(Output, Field->first);
            AppendCanonical(Output, Field->second);
        }
        Output.push_back('}');
        return;
    }
    }
}

constexpr std::array<std::uint32_t, 64> K{
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

std::uint32_t RotateRight(const std::uint32_t Value, const int Amount)
{
    return (Value >> Amount) | (Value << (32 - Amount));
}

std::string Sha256(std::string Input)
{
    const std::uint64_t BitLength = static_cast<std::uint64_t>(Input.size()) * 8;
    Input.push_back(static_cast<char>(0x80));
    while ((Input.size() % 64) != 56) Input.push_back('\0');
    for (int Shift = 56; Shift >= 0; Shift -= 8)
    {
        Input.push_back(static_cast<char>((BitLength >> Shift) & 0xff));
    }

    std::array<std::uint32_t, 8> Hash{
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    for (std::size_t Offset = 0; Offset < Input.size(); Offset += 64)
    {
        std::array<std::uint32_t, 64> Words{};
        for (std::size_t Index = 0; Index < 16; ++Index)
        {
            const auto* Bytes = reinterpret_cast<const unsigned char*>(Input.data() + Offset + Index * 4);
            Words[Index] = (static_cast<std::uint32_t>(Bytes[0]) << 24)
                | (static_cast<std::uint32_t>(Bytes[1]) << 16)
                | (static_cast<std::uint32_t>(Bytes[2]) << 8)
                | static_cast<std::uint32_t>(Bytes[3]);
        }
        for (std::size_t Index = 16; Index < 64; ++Index)
        {
            const std::uint32_t S0 = RotateRight(Words[Index - 15], 7)
                ^ RotateRight(Words[Index - 15], 18) ^ (Words[Index - 15] >> 3);
            const std::uint32_t S1 = RotateRight(Words[Index - 2], 17)
                ^ RotateRight(Words[Index - 2], 19) ^ (Words[Index - 2] >> 10);
            Words[Index] = Words[Index - 16] + S0 + Words[Index - 7] + S1;
        }
        auto [A, B, C, D, E, F, G, H] = Hash;
        for (std::size_t Index = 0; Index < 64; ++Index)
        {
            const std::uint32_t S1 = RotateRight(E, 6) ^ RotateRight(E, 11) ^ RotateRight(E, 25);
            const std::uint32_t Choice = (E & F) ^ ((~E) & G);
            const std::uint32_t Temp1 = H + S1 + Choice + K[Index] + Words[Index];
            const std::uint32_t S0 = RotateRight(A, 2) ^ RotateRight(A, 13) ^ RotateRight(A, 22);
            const std::uint32_t Majority = (A & B) ^ (A & C) ^ (B & C);
            const std::uint32_t Temp2 = S0 + Majority;
            H = G; G = F; F = E; E = D + Temp1;
            D = C; C = B; B = A; A = Temp1 + Temp2;
        }
        Hash[0] += A; Hash[1] += B; Hash[2] += C; Hash[3] += D;
        Hash[4] += E; Hash[5] += F; Hash[6] += G; Hash[7] += H;
    }
    std::ostringstream Output;
    Output << std::hex << std::setfill('0');
    for (const std::uint32_t Word : Hash) Output << std::setw(8) << Word;
    return Output.str();
}
}

std::string ComputeCanonicalHash(const FValue& Value)
{
    std::string Canonical;
    AppendCanonical(Canonical, Value);
    return Sha256(std::move(Canonical));
}
}
