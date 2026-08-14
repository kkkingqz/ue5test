#include "GV2ContentCore/Testing/Json5Conformance.h"

#include "GV2ContentCore/Json5Parser.h"

#include <algorithm>
#include <map>

namespace GV2ContentCore::Testing
{
namespace
{
struct FOutcome final
{
    std::optional<FValue> Value;
    std::vector<FDiagnostic> Diagnostics;

    bool operator==(const FOutcome&) const = default;
};

std::optional<std::map<std::string, FOutcome>> ParseOrder(
    const std::vector<const FJson5Fixture*>& OrderedFixtures,
    std::string& OutFailure)
{
    std::map<std::string, FOutcome> Outcomes;
    for (const FJson5Fixture* Fixture : OrderedFixtures)
    {
        FOutcome Outcome;
        Outcome.Value = ParseJson5(
            Fixture->Source,
            FParseLimits{},
            Outcome.Diagnostics,
            std::nullopt,
            std::nullopt,
            Fixture->RelativePath);

        if (Fixture->ExpectedDiagnosticCode.has_value())
        {
            if (Outcome.Value.has_value()
                || Outcome.Diagnostics.empty()
                || Outcome.Diagnostics.front().Code != *Fixture->ExpectedDiagnosticCode)
            {
                OutFailure = "Fixture did not produce expected diagnostic: " + Fixture->RelativePath;
                return std::nullopt;
            }
            if (*Fixture->ExpectedDiagnosticCode == "core:diagnostic.json5.duplicate_key"
                && (!Outcome.Diagnostics.front().Span.has_value()
                    || !Outcome.Diagnostics.front().RelatedSpan.has_value()))
            {
                OutFailure = "Duplicate-key fixture lacks primary/related source spans: " + Fixture->RelativePath;
                return std::nullopt;
            }
        }
        else if (!Outcome.Value.has_value() || !Outcome.Diagnostics.empty())
        {
            OutFailure = "Fixture did not parse successfully: " + Fixture->RelativePath;
            return std::nullopt;
        }

        if (!Outcomes.emplace(Fixture->RelativePath, std::move(Outcome)).second)
        {
            OutFailure = "Duplicate fixture path: " + Fixture->RelativePath;
            return std::nullopt;
        }
    }
    return Outcomes;
}
}

bool RunJson5FixtureConformance(
    const std::vector<FJson5Fixture>& Fixtures,
    std::string& OutFailure)
{
    std::vector<const FJson5Fixture*> Forward;
    Forward.reserve(Fixtures.size());
    for (const FJson5Fixture& Fixture : Fixtures)
    {
        Forward.push_back(&Fixture);
    }
    std::sort(
        Forward.begin(),
        Forward.end(),
        [](const FJson5Fixture* Left, const FJson5Fixture* Right)
        {
            return Left->RelativePath < Right->RelativePath;
        });

    auto ForwardOutcomes = ParseOrder(Forward, OutFailure);
    if (!ForwardOutcomes.has_value())
    {
        return false;
    }

    std::reverse(Forward.begin(), Forward.end());
    auto ReverseOutcomes = ParseOrder(Forward, OutFailure);
    if (!ReverseOutcomes.has_value())
    {
        return false;
    }
    if (*ForwardOutcomes != *ReverseOutcomes)
    {
        OutFailure = "Parser outcome changed when fixture order was reversed";
        return false;
    }

    OutFailure.clear();
    return true;
}
}
