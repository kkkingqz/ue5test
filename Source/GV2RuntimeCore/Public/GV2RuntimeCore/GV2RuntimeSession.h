#pragma once

#include "GV2RuntimeCore/GV2RuntimeCoreAPI.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace GV2RuntimeCore
{
struct FValue
{
    using FArray = std::vector<FValue>;
    using FObject = std::map<std::string, FValue, std::less<>>;
    using FStorage = std::variant<std::monostate, bool, std::int64_t, double, std::string, FArray, FObject>;

    FStorage Data;

    FValue() = default;
    explicit FValue(bool Value) : Data(Value) {}
    explicit FValue(std::int64_t Value) : Data(Value) {}
    explicit FValue(double Value) : Data(Value) {}
    explicit FValue(std::string Value) : Data(std::move(Value)) {}
    explicit FValue(FArray Value) : Data(std::move(Value)) {}
    explicit FValue(FObject Value) : Data(std::move(Value)) {}
};

struct FRuntimeFault
{
    std::string Code;
    std::string Message;
};

struct FCommandRequest
{
    std::string CommandId;
    FValue::FObject Args;
    std::int64_t Sequence = 0;
};

struct FSemanticInput
{
    std::int32_t SessionGeneration = 0;
    std::string UiInstanceId;
    std::int64_t Revision = 0;
    std::int64_t Sequence = 0;
    std::vector<std::string> NodeKeyPath;
    std::string ElementId;
    std::string CommandId;
    FValue::FObject Args;
};

struct FTextSpec
{
    std::string TextId;
    FValue::FObject Args;
    std::string Style;
};

struct FResourceReference
{
    std::string ResourceId;
};

struct FScreenField
{
    std::string FieldId;
    std::string SchemaId;
    FValue Value;
};

struct FScreenRequest
{
    std::string ScreenId;
    std::vector<FScreenField> Fields;
};

struct FRuntimeSource
{
    std::string Name;
    std::string Text;
};

class GV2_PORTABLE_API FRuntimeSession
{
public:
    FRuntimeSession();
    ~FRuntimeSession();

    FRuntimeSession(const FRuntimeSession&) = delete;
    FRuntimeSession& operator=(const FRuntimeSession&) = delete;

    bool Start(
        std::int32_t InSessionGeneration,
        const std::vector<FRuntimeSource>& Sources,
        FRuntimeFault& OutFault);
    bool Stop(FRuntimeFault* OutFault = nullptr);

    bool DispatchSemanticInput(const FSemanticInput& Input, FRuntimeFault& OutFault);
    bool DispatchCommand(const FCommandRequest& Request, FRuntimeFault& OutFault);
    bool TakePendingScreen(
        std::optional<FScreenRequest>& OutRequest,
        FRuntimeFault& OutFault);

    bool IsStarted() const;
    bool IsExecuting() const;
    std::int32_t GetSessionGeneration() const;

    static constexpr std::int32_t LuaReleaseNumber = 50408;

private:
    struct FImpl;
    std::unique_ptr<FImpl> Impl;
};
}
