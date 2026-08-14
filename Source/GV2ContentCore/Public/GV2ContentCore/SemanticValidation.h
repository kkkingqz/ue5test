#pragma once

#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/GV2ContentCore.h"
#include "GV2ContentCore/Value.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace GV2ContentCore
{
class GV2_CONTENT_CORE_API FSemanticCandidateView final
{
public:
    explicit FSemanticCandidateView(const FValue::FArray& InDefinitions)
        : Definitions(InDefinitions) {}
    const FValue* FindDefinition(std::string_view DefinitionId) const;
    const FValue::FArray& GetDefinitions() const { return Definitions; }

private:
    const FValue::FArray& Definitions;
};

struct FSemanticValidationContext final
{
    std::optional<std::string> PackageId;
    std::optional<std::uint32_t> PackageLoadIndex;
    std::optional<std::string> RelativeSource;
    std::optional<FSourceSpan> DefinitionSpan;
    std::optional<std::string> DefinitionId;
    std::optional<std::string> SchemaId;
    std::optional<std::int64_t> SchemaVersion;
};

class GV2_CONTENT_CORE_API ISemanticValidator
{
public:
    virtual ~ISemanticValidator() = default;
    virtual std::string_view GetId() const = 0;
    virtual void Validate(
        const FValue& Definition,
        const FSemanticCandidateView& Candidate,
        const FSemanticValidationContext& Context,
        std::vector<FDiagnostic>& OutDiagnostics) const = 0;
};

/** Non-owning registry frozen by the caller for the duration of BuildRepository(). */
class GV2_CONTENT_CORE_API FSemanticValidatorRegistry final
{
public:
    bool Register(const ISemanticValidator& Validator, std::vector<FDiagnostic>& OutDiagnostics);
    const ISemanticValidator* Find(std::string_view ValidatorId) const;
    std::vector<std::string> ListIds() const;

private:
    std::map<std::string, const ISemanticValidator*> Validators;
};

/** Built-in portable validators owned for process lifetime by GV2ContentCore. */
GV2_CONTENT_CORE_API const FSemanticValidatorRegistry& GetCoreSemanticValidatorRegistry();
}
