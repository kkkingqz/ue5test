#include "GV2ContentCore/SemanticValidation.h"

#include "GV2ContentCore/StableId.h"

namespace GV2ContentCore
{
namespace
{
class FPositiveItemPriceValidator final : public ISemanticValidator
{
public:
    std::string_view GetId() const override
    {
        return "core:validator.item.positive_price";
    }

    void Validate(
        const FValue& Definition,
        const FSemanticCandidateView& Candidate,
        const FSemanticValidationContext& Context,
        std::vector<FDiagnostic>& OutDiagnostics) const override
    {
        (void)Candidate;
        const FValue* Data = Definition.FindField("data");
        const FValue* Price = Data != nullptr ? Data->FindField("price") : nullptr;
        if (Price != nullptr && Price->IsInteger() && Price->AsInteger() > 0) return;

        FDiagnostic Diagnostic;
        Diagnostic.Code = "core:diagnostic.semantic.item.price_not_positive";
        Diagnostic.Message = "Item price must be positive";
        Diagnostic.PackageId = Context.PackageId;
        Diagnostic.PackageLoadIndex = Context.PackageLoadIndex;
        Diagnostic.RelativeSource = Context.RelativeSource;
        Diagnostic.DefinitionId = Context.DefinitionId;
        Diagnostic.SchemaId = Context.SchemaId;
        Diagnostic.SchemaVersion = Context.SchemaVersion;
        Diagnostic.JsonPointer = "/data/price";
        Diagnostic.Span = Context.DefinitionSpan;
        OutDiagnostics.push_back(std::move(Diagnostic));
    }
};
}

const FValue* FSemanticCandidateView::FindDefinition(const std::string_view DefinitionId) const
{
    for (const FValue& Definition : Definitions)
    {
        const FValue* Id = Definition.FindField("id");
        if (Id != nullptr && Id->IsString() && Id->AsString() == DefinitionId) return &Definition;
    }
    return nullptr;
}

bool FSemanticValidatorRegistry::Register(
    const ISemanticValidator& Validator,
    std::vector<FDiagnostic>& OutDiagnostics)
{
    const std::string Id(Validator.GetId());
    if (!FStableId::IsOfKind(Id, "validator"))
    {
        FDiagnostic Diagnostic;
        Diagnostic.Code = "core:diagnostic.semantic.validator.invalid_id";
        Diagnostic.Message = "Semantic validator ID must be a canonical validator Stable ID";
        OutDiagnostics.push_back(std::move(Diagnostic));
        return false;
    }
    if (!Validators.emplace(Id, &Validator).second)
    {
        FDiagnostic Diagnostic;
        Diagnostic.Code = "core:diagnostic.semantic.validator.duplicate_id";
        Diagnostic.Message = "Semantic validator ID is already registered";
        OutDiagnostics.push_back(std::move(Diagnostic));
        return false;
    }
    return true;
}

const ISemanticValidator* FSemanticValidatorRegistry::Find(const std::string_view ValidatorId) const
{
    const auto Found = Validators.find(std::string(ValidatorId));
    return Found == Validators.end() ? nullptr : Found->second;
}

std::vector<std::string> FSemanticValidatorRegistry::ListIds() const
{
    std::vector<std::string> Result;
    Result.reserve(Validators.size());
    for (const auto& [Id, Validator] : Validators)
    {
        (void)Validator;
        Result.push_back(Id);
    }
    return Result;
}

const FSemanticValidatorRegistry& GetCoreSemanticValidatorRegistry()
{
    static const FPositiveItemPriceValidator PositiveItemPrice;
    static const FSemanticValidatorRegistry Registry = []
    {
        FSemanticValidatorRegistry Result;
        std::vector<FDiagnostic> Diagnostics;
        Result.Register(PositiveItemPrice, Diagnostics);
        return Result;
    }();
    return Registry;
}
}
