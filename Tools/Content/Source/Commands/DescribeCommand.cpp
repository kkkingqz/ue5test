#include "Commands/DescribeCommand.h"
#include "Support/CliOutput.h"
#include "Support/PackageLoader.h"
#include "GV2ContentCore/ExtensionSchema.h"
#include "GV2ContentCore/FieldValidation.h"
#include "GV2ContentCore/Json5Parser.h"
#include "GV2ContentCore/PackageDescriptor.h"
#include "GV2ContentCore/SchemaRegistry.h"
#include "GV2ContentHostSupport/PackageDiscovery.h"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace GV2ContentCli
{

namespace
{

std::string FormatValueString(const GV2ContentCore::FValue& Value)
{
    if (Value.IsString()) return Value.AsString();
    if (Value.IsInteger()) return std::to_string(Value.AsInteger());
    if (Value.IsBoolean()) return Value.AsBoolean() ? "true" : "false";
    if (Value.IsNumber()) return FormatDouble(Value.AsNumber());
    return "null";
}

std::string FormatCompactSpecSummary(const GV2ContentCore::FCompiledFieldSpec& Spec)
{
    if (Spec.Kind == GV2ContentCore::EFieldKind::Scalar && Spec.Scalar.has_value())
    {
        switch (Spec.Scalar->Kind)
        {
        case GV2ContentCore::EScalarFieldKind::Boolean: return "boolean";
        case GV2ContentCore::EScalarFieldKind::Integer: return "int64";
        case GV2ContentCore::EScalarFieldKind::Number: return "double";
        case GV2ContentCore::EScalarFieldKind::String: return "string";
        case GV2ContentCore::EScalarFieldKind::Enum:
        {
            std::string S = "enum([";
            for (std::size_t i = 0; i < Spec.Scalar->EnumValues.size(); ++i)
            {
                if (i != 0) S += ", ";
                S += FormatValueString(Spec.Scalar->EnumValues[i]);
            }
            S += "])";
            return S;
        }
        }
    }
    if (Spec.Kind == GV2ContentCore::EFieldKind::Reference)
    {
        return "ref(" + Spec.ExpectedStableIdKind + ")";
    }
    if (Spec.Kind == GV2ContentCore::EFieldKind::TextId)
    {
        return "text_id";
    }
    if (Spec.Kind == GV2ContentCore::EFieldKind::ResourceReference)
    {
        return "resource_ref(" + Spec.ResourceClass + ")";
    }
    if (Spec.Kind == GV2ContentCore::EFieldKind::Array)
    {
        return "array(" + (Spec.Items ? FormatCompactSpecSummary(*Spec.Items) : "") + ")";
    }
    if (Spec.Kind == GV2ContentCore::EFieldKind::Map)
    {
        return "map(" + (Spec.MapKeys ? FormatCompactSpecSummary(*Spec.MapKeys) : "") + " -> "
                      + (Spec.MapValues ? FormatCompactSpecSummary(*Spec.MapValues) : "") + ")";
    }
    if (Spec.Kind == GV2ContentCore::EFieldKind::Object)
    {
        return "object";
    }
    if (Spec.Kind == GV2ContentCore::EFieldKind::Union)
    {
        return "union(" + Spec.Discriminator + ")";
    }
    return "unknown";
}

void FormatFieldDetailsText(
    std::ostream& Out,
    const std::string& FieldName,
    bool bRequired,
    const GV2ContentCore::FCompiledFieldSpec& Spec,
    int IndentLevel)
{
    std::string Indent(IndentLevel * 2, ' ');
    Out << Indent << FieldName << ": ";

    std::vector<std::string> Constraints;
    std::string KindName;

    if (Spec.Kind == GV2ContentCore::EFieldKind::Scalar && Spec.Scalar.has_value())
    {
        const auto& S = *Spec.Scalar;
        switch (S.Kind)
        {
        case GV2ContentCore::EScalarFieldKind::Boolean:
            KindName = "boolean";
            break;
        case GV2ContentCore::EScalarFieldKind::Integer:
            KindName = "int64";
            if (S.MinimumInteger.has_value()) Constraints.push_back("min=" + std::to_string(*S.MinimumInteger));
            if (S.MaximumInteger.has_value()) Constraints.push_back("max=" + std::to_string(*S.MaximumInteger));
            break;
        case GV2ContentCore::EScalarFieldKind::Number:
            KindName = "double";
            if (S.MinimumNumber.has_value()) Constraints.push_back("min=" + FormatDouble(*S.MinimumNumber));
            if (S.MaximumNumber.has_value()) Constraints.push_back("max=" + FormatDouble(*S.MaximumNumber));
            if (S.bMinimumExclusive) Constraints.push_back("min_exclusive=true");
            if (S.bMaximumExclusive) Constraints.push_back("max_exclusive=true");
            break;
        case GV2ContentCore::EScalarFieldKind::String:
            KindName = "string";
            if (S.MinimumLength.has_value()) Constraints.push_back("min_length=" + std::to_string(*S.MinimumLength));
            if (S.MaximumLength.has_value()) Constraints.push_back("max_length=" + std::to_string(*S.MaximumLength));
            if (S.Pattern.has_value()) Constraints.push_back("pattern=" + *S.Pattern);
            if (S.Format.has_value()) Constraints.push_back("format=" + *S.Format);
            break;
        case GV2ContentCore::EScalarFieldKind::Enum:
        {
            KindName = "enum";
            std::string Vals = "[";
            for (std::size_t i = 0; i < S.EnumValues.size(); ++i)
            {
                if (i != 0) Vals += ", ";
                Vals += FormatValueString(S.EnumValues[i]);
            }
            Vals += "]";
            Constraints.push_back("values=" + Vals);
            break;
        }
        }
    }
    else if (Spec.Kind == GV2ContentCore::EFieldKind::Array)
    {
        KindName = "array";
        if (Spec.MinimumSize.has_value()) Constraints.push_back("min_items=" + std::to_string(*Spec.MinimumSize));
        if (Spec.MaximumSize.has_value()) Constraints.push_back("max_items=" + std::to_string(*Spec.MaximumSize));
        if (Spec.bUnique) Constraints.push_back("unique=true");
        if (Spec.Items != nullptr)
        {
            Constraints.push_back("items=" + FormatCompactSpecSummary(*Spec.Items));
        }
    }
    else if (Spec.Kind == GV2ContentCore::EFieldKind::Map)
    {
        KindName = "map";
        if (Spec.MinimumSize.has_value()) Constraints.push_back("min_entries=" + std::to_string(*Spec.MinimumSize));
        if (Spec.MaximumSize.has_value()) Constraints.push_back("max_entries=" + std::to_string(*Spec.MaximumSize));
        if (Spec.MapKeys != nullptr) Constraints.push_back("keys=" + FormatCompactSpecSummary(*Spec.MapKeys));
        if (Spec.MapValues != nullptr) Constraints.push_back("values=" + FormatCompactSpecSummary(*Spec.MapValues));
    }
    else if (Spec.Kind == GV2ContentCore::EFieldKind::Object)
    {
        KindName = "object";
    }
    else if (Spec.Kind == GV2ContentCore::EFieldKind::Union)
    {
        KindName = "union";
        Constraints.push_back("discriminator=" + Spec.Discriminator);
    }
    else if (Spec.Kind == GV2ContentCore::EFieldKind::Reference)
    {
        KindName = "ref";
        Constraints.push_back("target_kind=" + Spec.ExpectedStableIdKind);
    }
    else if (Spec.Kind == GV2ContentCore::EFieldKind::TextId)
    {
        KindName = "text_id";
    }
    else if (Spec.Kind == GV2ContentCore::EFieldKind::ResourceReference)
    {
        KindName = "resource_ref";
        Constraints.push_back("resource_class=" + Spec.ResourceClass);
        if (Spec.bBootstrapRequired) Constraints.push_back("bootstrap_required=true");
    }

    Out << KindName << " (" << (bRequired ? "required" : "optional");
    if (Spec.bNullable) Out << ", nullable";
    for (const auto& C : Constraints)
    {
        Out << ", " << C;
    }
    Out << ")\n";

    if (Spec.Kind == GV2ContentCore::EFieldKind::Object)
    {
        for (const auto& ChildField : Spec.Fields)
        {
            if (ChildField.Spec != nullptr)
            {
                FormatFieldDetailsText(Out, ChildField.Name, ChildField.bRequired, *ChildField.Spec, IndentLevel + 1);
            }
        }
    }
}

void WriteFieldSpecJson(std::ostream& Out, const GV2ContentCore::FCompiledFieldSpec& Spec)
{
    Out << '{';
    bool bFirst = true;
    const auto Comma = [&]() {
        if (!bFirst) Out << ',';
        bFirst = false;
    };

    Comma();
    Out << "\"kind\":";
    switch (Spec.Kind)
    {
    case GV2ContentCore::EFieldKind::Scalar:
        if (Spec.Scalar.has_value())
        {
            switch (Spec.Scalar->Kind)
            {
            case GV2ContentCore::EScalarFieldKind::Boolean: Out << "\"boolean\""; break;
            case GV2ContentCore::EScalarFieldKind::Integer: Out << "\"int64\""; break;
            case GV2ContentCore::EScalarFieldKind::Number: Out << "\"double\""; break;
            case GV2ContentCore::EScalarFieldKind::String: Out << "\"string\""; break;
            case GV2ContentCore::EScalarFieldKind::Enum: Out << "\"enum\""; break;
            }
        }
        else
        {
            Out << "\"scalar\"";
        }
        break;
    case GV2ContentCore::EFieldKind::Array: Out << "\"array\""; break;
    case GV2ContentCore::EFieldKind::Map: Out << "\"map\""; break;
    case GV2ContentCore::EFieldKind::Object: Out << "\"object\""; break;
    case GV2ContentCore::EFieldKind::Union: Out << "\"union\""; break;
    case GV2ContentCore::EFieldKind::Reference: Out << "\"ref\""; break;
    case GV2ContentCore::EFieldKind::TextId: Out << "\"text_id\""; break;
    case GV2ContentCore::EFieldKind::ResourceReference: Out << "\"resource_ref\""; break;
    }

    if (Spec.bNullable)
    {
        Comma();
        Out << "\"nullable\":true";
    }
    if (Spec.DefaultValue.has_value())
    {
        Comma();
        Out << "\"default\":";
        WriteJsonValue(Out, *Spec.DefaultValue);
    }

    if (Spec.Kind == GV2ContentCore::EFieldKind::Scalar && Spec.Scalar.has_value())
    {
        const auto& S = *Spec.Scalar;
        if (S.Kind == GV2ContentCore::EScalarFieldKind::Integer)
        {
            if (S.MinimumInteger.has_value())
            {
                Comma();
                Out << "\"min\":" << *S.MinimumInteger;
            }
            if (S.MaximumInteger.has_value())
            {
                Comma();
                Out << "\"max\":" << *S.MaximumInteger;
            }
        }
        else if (S.Kind == GV2ContentCore::EScalarFieldKind::Number)
        {
            if (S.MinimumNumber.has_value())
            {
                Comma();
                Out << "\"min\":" << FormatDouble(*S.MinimumNumber);
            }
            if (S.MaximumNumber.has_value())
            {
                Comma();
                Out << "\"max\":" << FormatDouble(*S.MaximumNumber);
            }
            if (S.bMinimumExclusive)
            {
                Comma();
                Out << "\"min_exclusive\":true";
            }
            if (S.bMaximumExclusive)
            {
                Comma();
                Out << "\"max_exclusive\":true";
            }
        }
        else if (S.Kind == GV2ContentCore::EScalarFieldKind::String)
        {
            if (S.MinimumLength.has_value())
            {
                Comma();
                Out << "\"min_length\":" << *S.MinimumLength;
            }
            if (S.MaximumLength.has_value())
            {
                Comma();
                Out << "\"max_length\":" << *S.MaximumLength;
            }
            if (S.Pattern.has_value())
            {
                Comma();
                Out << "\"pattern\":";
                WriteJsonEscapedString(Out, *S.Pattern);
            }
            if (S.Format.has_value())
            {
                Comma();
                Out << "\"format\":";
                WriteJsonEscapedString(Out, *S.Format);
            }
        }
        else if (S.Kind == GV2ContentCore::EScalarFieldKind::Enum)
        {
            Comma();
            Out << "\"values\":[";
            for (std::size_t i = 0; i < S.EnumValues.size(); ++i)
            {
                if (i != 0) Out << ',';
                WriteJsonValue(Out, S.EnumValues[i]);
            }
            Out << ']';
        }
    }
    else if (Spec.Kind == GV2ContentCore::EFieldKind::Array)
    {
        if (Spec.MinimumSize.has_value())
        {
            Comma();
            Out << "\"min_items\":" << *Spec.MinimumSize;
        }
        if (Spec.MaximumSize.has_value())
        {
            Comma();
            Out << "\"max_items\":" << *Spec.MaximumSize;
        }
        if (Spec.bUnique)
        {
            Comma();
            Out << "\"unique\":true";
        }
        if (Spec.Items != nullptr)
        {
            Comma();
            Out << "\"items\":";
            WriteFieldSpecJson(Out, *Spec.Items);
        }
    }
    else if (Spec.Kind == GV2ContentCore::EFieldKind::Map)
    {
        if (Spec.MinimumSize.has_value())
        {
            Comma();
            Out << "\"min_entries\":" << *Spec.MinimumSize;
        }
        if (Spec.MaximumSize.has_value())
        {
            Comma();
            Out << "\"max_entries\":" << *Spec.MaximumSize;
        }
        if (Spec.MapKeys != nullptr)
        {
            Comma();
            Out << "\"keys\":";
            WriteFieldSpecJson(Out, *Spec.MapKeys);
        }
        if (Spec.MapValues != nullptr)
        {
            Comma();
            Out << "\"values\":";
            WriteFieldSpecJson(Out, *Spec.MapValues);
        }
    }
    else if (Spec.Kind == GV2ContentCore::EFieldKind::Object)
    {
        Comma();
        Out << "\"fields\":{";
        for (std::size_t i = 0; i < Spec.Fields.size(); ++i)
        {
            if (i != 0) Out << ',';
            WriteJsonEscapedString(Out, Spec.Fields[i].Name);
            Out << ":{";
            Out << "\"required\":" << (Spec.Fields[i].bRequired ? "true" : "false");
            if (Spec.Fields[i].Spec != nullptr)
            {
                std::ostringstream SpecStream;
                WriteFieldSpecJson(SpecStream, *Spec.Fields[i].Spec);
                std::string Inner = SpecStream.str();
                if (Inner.size() >= 2 && Inner.front() == '{' && Inner.back() == '}')
                {
                    std::string Rest = Inner.substr(1, Inner.size() - 2);
                    if (!Rest.empty())
                    {
                        Out << ',' << Rest;
                    }
                }
            }
            Out << '}';
        }
        Out << '}';
    }
    else if (Spec.Kind == GV2ContentCore::EFieldKind::Union)
    {
        Comma();
        Out << "\"discriminator\":";
        WriteJsonEscapedString(Out, Spec.Discriminator);
        Comma();
        Out << "\"variants\":{";
        for (std::size_t i = 0; i < Spec.Variants.size(); ++i)
        {
            if (i != 0) Out << ',';
            WriteJsonEscapedString(Out, Spec.Variants[i].DiscriminatorValue);
            Out << ':';
            if (Spec.Variants[i].Spec != nullptr)
            {
                WriteFieldSpecJson(Out, *Spec.Variants[i].Spec);
            }
            else
            {
                Out << "{}";
            }
        }
        Out << '}';
    }
    else if (Spec.Kind == GV2ContentCore::EFieldKind::Reference)
    {
        Comma();
        Out << "\"target_kind\":";
        WriteJsonEscapedString(Out, Spec.ExpectedStableIdKind);
    }
    else if (Spec.Kind == GV2ContentCore::EFieldKind::TextId)
    {
        // kind already written
    }
    else if (Spec.Kind == GV2ContentCore::EFieldKind::ResourceReference)
    {
        Comma();
        Out << "\"resource_class\":";
        WriteJsonEscapedString(Out, Spec.ResourceClass);
        if (Spec.bBootstrapRequired)
        {
            Comma();
            Out << "\"bootstrap_required\":true";
        }
    }

    Out << '}';
}

} // namespace

int RunDescribe(const std::vector<std::string>& Positional, EOutputFormat Format)
{
    if (Positional.size() != 2)
    {
        std::cerr << "usage: gv2-content describe <package-root> <definition-type> [--format=text|json]\n";
        return static_cast<int>(EExitCode::ToolFailure);
    }

    const std::filesystem::path RawRoot = Positional[0];
    const std::string& DefinitionType = Positional[1];

    std::error_code Ec;
    const std::filesystem::path NormalizedRoot = std::filesystem::weakly_canonical(RawRoot, Ec);
    const std::filesystem::path Root = (!Ec && !NormalizedRoot.empty()) ? NormalizedRoot : RawRoot;

    if (!std::filesystem::is_directory(Root, Ec) || Ec)
    {
        std::cerr << "gv2-content: package root not found or not a directory\n";
        return static_cast<int>(EExitCode::ToolFailure);
    }

    std::vector<GV2ContentCore::FDiagnostic> Diagnostics;
    std::optional<GV2ContentCore::FPackageDescriptor> Descriptor =
        GV2ContentHostSupport::DiscoverPackageFromDirectory(Root, Diagnostics);
    if (!Descriptor)
    {
        return EmitDiagnosticsFailure(Diagnostics, Format);
    }

    const GV2ContentCore::FSchemaBinding* MatchingBinding = nullptr;
    for (const GV2ContentCore::FSchemaBinding& Binding : Descriptor->GetSchemaBindings())
    {
        if (Binding.GetDefinitionType() == DefinitionType)
        {
            MatchingBinding = &Binding;
            break;
        }
    }

    if (MatchingBinding == nullptr)
    {
        if (Format == EOutputFormat::Json)
        {
            std::cout << "{\"status\":\"error\",\"code\":\"unknown_definition_type\",\"message\":\"Unknown definition type '";
            std::cout << DefinitionType << "' in package '" << Descriptor->GetPackageId() << "'\",\"definition_type\":\""
                      << DefinitionType << "\"}\n";
        }
        else
        {
            std::cerr << "gv2-content: unknown definition type '" << DefinitionType
                      << "' in package '" << Descriptor->GetPackageId() << "'\n";
        }
        return static_cast<int>(EExitCode::ToolFailure);
    }

    FFilesystemContentSourceProvider Provider(Root, Descriptor->GetPackageId());
    std::optional<std::string> SchemaSource = Provider.ReadSource(
        Descriptor->GetPackageId(), MatchingBinding->GetRelativePath());
    if (!SchemaSource)
    {
        std::cerr << "gv2-content: failed to read schema source '" << MatchingBinding->GetRelativePath() << "'\n";
        return static_cast<int>(EExitCode::ToolFailure);
    }

    GV2ContentCore::FParseLimits Limits;
    auto SchemaDoc = GV2ContentCore::ParseJson5Document(
        *SchemaSource,
        Limits,
        Diagnostics,
        Descriptor->GetPackageId(),
        Descriptor->GetLoadIndex(),
        MatchingBinding->GetRelativePath());

    if (!SchemaDoc)
    {
        return EmitDiagnosticsFailure(Diagnostics, Format);
    }

    auto SchemaResourceOpt = GV2ContentCore::ParseSchemaResource(
        *SchemaDoc,
        *MatchingBinding,
        Descriptor->GetPackageId(),
        Descriptor->GetLoadIndex(),
        MatchingBinding->GetRelativePath(),
        Diagnostics);

    if (!SchemaResourceOpt || !Diagnostics.empty())
    {
        return EmitDiagnosticsFailure(Diagnostics, Format);
    }

    const GV2ContentCore::FSchemaResource& Schema = *SchemaResourceOpt;

    std::vector<GV2ContentCore::FExtensionSchemaResource> ExtensionSchemas;
    for (const GV2ContentCore::FExtensionSchemaBinding& ExtBinding : Descriptor->GetExtensionSchemaBindings())
    {
        if (ExtBinding.GetDefinitionType() == DefinitionType)
        {
            std::optional<std::string> ExtSource = Provider.ReadSource(
                Descriptor->GetPackageId(), ExtBinding.GetRelativePath());
            if (!ExtSource)
            {
                std::cerr << "gv2-content: failed to read extension schema source '" << ExtBinding.GetRelativePath() << "'\n";
                return static_cast<int>(EExitCode::ToolFailure);
            }

            auto ExtDoc = GV2ContentCore::ParseJson5Document(
                *ExtSource,
                Limits,
                Diagnostics,
                Descriptor->GetPackageId(),
                Descriptor->GetLoadIndex(),
                ExtBinding.GetRelativePath());

            if (!ExtDoc)
            {
                return EmitDiagnosticsFailure(Diagnostics, Format);
            }

            auto ExtResourceOpt = GV2ContentCore::ParseExtensionSchemaResource(
                *ExtDoc,
                ExtBinding,
                Descriptor->GetPackageId(),
                Descriptor->GetLoadIndex(),
                ExtBinding.GetRelativePath(),
                Diagnostics);

            if (!ExtResourceOpt || !Diagnostics.empty())
            {
                return EmitDiagnosticsFailure(Diagnostics, Format);
            }

            ExtensionSchemas.push_back(std::move(*ExtResourceOpt));
        }
    }

    if (Format == EOutputFormat::Json)
    {
        std::cout << "{\"status\":\"ok\",\"definition_type\":";
        WriteJsonEscapedString(std::cout, DefinitionType);
        std::cout << ",\"schema_id\":";
        WriteJsonEscapedString(std::cout, Schema.GetSchemaId());
        std::cout << ",\"schema_version\":" << Schema.GetKey().SchemaVersion;
        std::cout << ",\"package_id\":";
        WriteJsonEscapedString(std::cout, Schema.GetPackageId());
        std::cout << ",\"relative_source\":";
        WriteJsonEscapedString(std::cout, Schema.GetRelativeSource());
        std::cout << ",\"semantic_validators\":[";
        for (std::size_t i = 0; i < Schema.GetSemanticValidators().size(); ++i)
        {
            if (i != 0) std::cout << ",";
            WriteJsonEscapedString(std::cout, Schema.GetSemanticValidators()[i]);
        }
        std::cout << "],\"fields\":{";
        if (Schema.GetCompiledRootSpec() != nullptr && Schema.GetCompiledRootSpec()->Kind == GV2ContentCore::EFieldKind::Object)
        {
            const auto& Fields = Schema.GetCompiledRootSpec()->Fields;
            for (std::size_t i = 0; i < Fields.size(); ++i)
            {
                if (i != 0) std::cout << ",";
                WriteJsonEscapedString(std::cout, Fields[i].Name);
                std::cout << ":{";
                std::cout << "\"required\":" << (Fields[i].bRequired ? "true" : "false");
                if (Fields[i].Spec != nullptr)
                {
                    std::ostringstream SpecStream;
                    WriteFieldSpecJson(SpecStream, *Fields[i].Spec);
                    std::string Inner = SpecStream.str();
                    if (Inner.size() >= 2 && Inner.front() == '{' && Inner.back() == '}')
                    {
                        std::string Rest = Inner.substr(1, Inner.size() - 2);
                        if (!Rest.empty())
                        {
                            std::cout << "," << Rest;
                        }
                    }
                }
                std::cout << "}";
            }
        }
        std::cout << "},\"extensions\":[";
        for (std::size_t i = 0; i < ExtensionSchemas.size(); ++i)
        {
            if (i != 0) std::cout << ",";
            const auto& Ext = ExtensionSchemas[i];
            std::cout << "{\"namespace\":";
            WriteJsonEscapedString(std::cout, Ext.GetKey().ExtensionNamespace);
            std::cout << ",\"site\":";
            WriteJsonEscapedString(std::cout, std::string(GV2ContentCore::ToString(Ext.GetKey().Site)));
            std::cout << ",\"schema_id\":";
            WriteJsonEscapedString(std::cout, Ext.GetSchemaId());
            std::cout << ",\"schema_version\":" << Ext.GetKey().SchemaVersion;
            std::cout << ",\"relative_source\":";
            WriteJsonEscapedString(std::cout, Ext.GetRelativeSource());
            std::cout << ",\"fields\":{";
            if (Ext.GetCompiledRootSpec() != nullptr && Ext.GetCompiledRootSpec()->Kind == GV2ContentCore::EFieldKind::Object)
            {
                const auto& Fields = Ext.GetCompiledRootSpec()->Fields;
                for (std::size_t j = 0; j < Fields.size(); ++j)
                {
                    if (j != 0) std::cout << ",";
                    WriteJsonEscapedString(std::cout, Fields[j].Name);
                    std::cout << ":{";
                    std::cout << "\"required\":" << (Fields[j].bRequired ? "true" : "false");
                    if (Fields[j].Spec != nullptr)
                    {
                        std::ostringstream SpecStream;
                        WriteFieldSpecJson(SpecStream, *Fields[j].Spec);
                        std::string Inner = SpecStream.str();
                        if (Inner.size() >= 2 && Inner.front() == '{' && Inner.back() == '}')
                        {
                            std::string Rest = Inner.substr(1, Inner.size() - 2);
                            if (!Rest.empty())
                            {
                                std::cout << "," << Rest;
                            }
                        }
                    }
                    std::cout << "}";
                }
            }
            std::cout << "}}";
        }
        std::cout << "]}\n";
    }
    else
    {
        std::cout << "definition_type: " << DefinitionType << "\n";
        std::cout << "schema_id: " << Schema.GetSchemaId() << "\n";
        std::cout << "schema_version: " << Schema.GetKey().SchemaVersion << "\n";
        std::cout << "package: " << Schema.GetPackageId() << " (" << Schema.GetRelativeSource() << ")\n";
        std::cout << "semantic_validators:";
        if (Schema.GetSemanticValidators().empty())
        {
            std::cout << " none\n";
        }
        else
        {
            for (const auto& Val : Schema.GetSemanticValidators())
            {
                std::cout << " " << Val;
            }
            std::cout << "\n";
        }

        std::cout << "\nfields:\n";
        if (Schema.GetCompiledRootSpec() != nullptr && Schema.GetCompiledRootSpec()->Kind == GV2ContentCore::EFieldKind::Object)
        {
            for (const auto& Field : Schema.GetCompiledRootSpec()->Fields)
            {
                if (Field.Spec != nullptr)
                {
                    FormatFieldDetailsText(std::cout, Field.Name, Field.bRequired, *Field.Spec, 1);
                }
            }
        }

        std::cout << "\nextensions:";
        if (ExtensionSchemas.empty())
        {
            std::cout << " none\n";
        }
        else
        {
            std::cout << "\n";
            for (const auto& Ext : ExtensionSchemas)
            {
                std::cout << "  " << Ext.GetKey().ExtensionNamespace
                          << " (site=" << GV2ContentCore::ToString(Ext.GetKey().Site)
                          << ", schema_id=" << Ext.GetSchemaId()
                          << ", version=" << Ext.GetKey().SchemaVersion << "):\n";
                if (Ext.GetCompiledRootSpec() != nullptr && Ext.GetCompiledRootSpec()->Kind == GV2ContentCore::EFieldKind::Object)
                {
                    std::cout << "    fields:\n";
                    for (const auto& Field : Ext.GetCompiledRootSpec()->Fields)
                    {
                        if (Field.Spec != nullptr)
                        {
                            FormatFieldDetailsText(std::cout, Field.Name, Field.bRequired, *Field.Spec, 3);
                        }
                    }
                }
            }
        }
    }

    return static_cast<int>(EExitCode::Success);
}

} // namespace GV2ContentCli
