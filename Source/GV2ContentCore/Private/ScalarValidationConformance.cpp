#include "GV2ContentCore/Testing/ScalarValidationConformance.h"

#include "GV2ContentCore/FieldValidation.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/ParseLimits.h"
#include "GV2ContentCore/ScalarValidation.h"

#include <optional>
#include <string>
#include <vector>

namespace GV2ContentCore::Testing
{
std::string RunScalarValidationConformance()
{
    FValidationDiagnosticContext Context;
    Context.PackageId = "core";
    Context.RelativeSource = "scalar.self_test.json5";
    Context.SchemaId = "core:schema.scalar.self_test";

    // 1. Integer range validation
    std::vector<FDiagnostic> Diagnostics;
    auto IntDoc = ParseJson5Document("{ kind: 'int64', min: 1, max: 3 }", FParseLimits{}, Diagnostics);
    if (!IntDoc.has_value() || !Diagnostics.empty())
    {
        return "scalar_validation.parse_int_spec";
    }
    auto IntegerSpec = CompileScalarFieldSpec(IntDoc->GetRootValue(), &*IntDoc, "", Context, Diagnostics);
    if (!IntegerSpec.has_value() || !Diagnostics.empty())
    {
        return "scalar_validation.compile_int_spec";
    }

    if (!ValidateScalarValue(FValue(2), *IntegerSpec, nullptr, "/data", Context, Diagnostics))
    {
        return "scalar_validation.valid_int_value";
    }

    Diagnostics.clear();
    if (ValidateScalarValue(FValue(2.0), *IntegerSpec, nullptr, "/data", Context, Diagnostics)
        || Diagnostics.empty()
        || Diagnostics.front().Code != "core:diagnostic.schema.value.type_mismatch")
    {
        return "scalar_validation.int_type_mismatch_rejected";
    }

    Diagnostics.clear();
    if (ValidateScalarValue(FValue(static_cast<std::int64_t>(0)), *IntegerSpec, nullptr, "/data", Context, Diagnostics)
        || Diagnostics.empty()
        || Diagnostics.front().Code != "core:diagnostic.schema.value.constraint_failed")
    {
        return "scalar_validation.int_out_of_range_rejected";
    }

    // 2. Double range validation
    Diagnostics.clear();
    auto DoubleDoc = ParseJson5Document("{ kind: 'number', min: 0.5, max: 2.5 }", FParseLimits{}, Diagnostics);
    if (!DoubleDoc.has_value() || !Diagnostics.empty())
    {
        return "scalar_validation.parse_double_spec";
    }
    auto DoubleSpec = CompileScalarFieldSpec(DoubleDoc->GetRootValue(), &*DoubleDoc, "", Context, Diagnostics);
    if (!DoubleSpec.has_value() || !Diagnostics.empty())
    {
        return "scalar_validation.compile_double_spec";
    }
    if (!ValidateScalarValue(FValue(1.5), *DoubleSpec, nullptr, "/data", Context, Diagnostics))
    {
        return "scalar_validation.valid_double_value";
    }

    Diagnostics.clear();
    if (ValidateScalarValue(FValue(3.0), *DoubleSpec, nullptr, "/data", Context, Diagnostics)
        || Diagnostics.empty()
        || Diagnostics.front().Code != "core:diagnostic.schema.value.constraint_failed")
    {
        return "scalar_validation.double_out_of_range_rejected";
    }

    // 3. Boolean validation
    Diagnostics.clear();
    auto BoolDoc = ParseJson5Document("{ kind: 'bool' }", FParseLimits{}, Diagnostics);
    if (!BoolDoc.has_value() || !Diagnostics.empty())
    {
        return "scalar_validation.parse_bool_spec";
    }
    auto BoolSpec = CompileScalarFieldSpec(BoolDoc->GetRootValue(), &*BoolDoc, "", Context, Diagnostics);
    if (!BoolSpec.has_value()
        || !ValidateScalarValue(FValue(true), *BoolSpec, nullptr, "/data", Context, Diagnostics)
        || !ValidateScalarValue(FValue(false), *BoolSpec, nullptr, "/data", Context, Diagnostics))
    {
        return "scalar_validation.valid_bool_value";
    }

    // 4. String format and pattern validation
    Diagnostics.clear();
    auto StringDoc = ParseJson5Document(
        "{ kind: 'string', min_length: 2, pattern: '^[a-z_]+$', format: 'stable_id_segment' }",
        FParseLimits{}, Diagnostics);
    if (!StringDoc.has_value() || !Diagnostics.empty())
    {
        return "scalar_validation.parse_string_spec";
    }
    auto StringSpec = CompileScalarFieldSpec(StringDoc->GetRootValue(), &*StringDoc, "", Context, Diagnostics);
    if (!StringSpec.has_value() || !Diagnostics.empty())
    {
        return "scalar_validation.compile_string_spec";
    }
    if (!ValidateScalarValue(FValue("valid_name"), *StringSpec, nullptr, "/data", Context, Diagnostics))
    {
        return "scalar_validation.valid_string_value";
    }

    Diagnostics.clear();
    if (ValidateScalarValue(FValue("1invalid"), *StringSpec, nullptr, "/data", Context, Diagnostics)
        || Diagnostics.empty()
        || Diagnostics.front().Code != "core:diagnostic.schema.value.constraint_failed")
    {
        return "scalar_validation.invalid_string_format_rejected";
    }

    Diagnostics.clear();
    if (ValidateScalarValue(FValue(""), *StringSpec, nullptr, "/data", Context, Diagnostics)
        || Diagnostics.empty()
        || Diagnostics.front().Code != "core:diagnostic.schema.value.constraint_failed")
    {
        return "scalar_validation.empty_string_min_length_rejected";
    }

    // 5. Enum validation with nullable support
    Diagnostics.clear();
    auto EnumDoc = ParseJson5Document(
        "{ kind: 'enum', nullable: true, values: ['small', 'large'] }",
        FParseLimits{}, Diagnostics);
    if (!EnumDoc.has_value() || !Diagnostics.empty())
    {
        return "scalar_validation.parse_enum_spec";
    }
    auto EnumSpec = CompileScalarFieldSpec(EnumDoc->GetRootValue(), &*EnumDoc, "", Context, Diagnostics);
    if (!EnumSpec.has_value() || !Diagnostics.empty())
    {
        return "scalar_validation.compile_enum_spec";
    }
    if (!ValidateScalarValue(FValue("large"), *EnumSpec, nullptr, "/data", Context, Diagnostics)
        || !ValidateScalarValue(FValue::MakeNull(), *EnumSpec, nullptr, "/data", Context, Diagnostics))
    {
        return "scalar_validation.valid_enum_and_null";
    }

    Diagnostics.clear();
    if (ValidateScalarValue(FValue("huge"), *EnumSpec, nullptr, "/data", Context, Diagnostics)
        || Diagnostics.empty()
        || Diagnostics.front().Code != "core:diagnostic.schema.value.enum_not_allowed")
    {
        return "scalar_validation.invalid_enum_value_rejected";
    }

    return "";
}
} // namespace GV2ContentCore::Testing
