#include "GV2ContentCore/BuildResult.h"

#include <algorithm>

namespace GV2ContentCore
{
    FBuildResult::FBuildResult(FSuccessState InState)
        : State(std::move(InState))
    {
    }

    FBuildResult::FBuildResult(FFailureState InState)
        : State(std::move(InState))
    {
    }

    FBuildResult FBuildResult::Success(FCandidate Candidate)
    {
        return FBuildResult(FSuccessState{ std::move(Candidate) });
    }

    FBuildResult FBuildResult::Failure(FDiagnosticList Diagnostics)
    {
        if (Diagnostics.empty())
        {
            throw std::invalid_argument("A failed build result requires at least one diagnostic");
        }
        std::sort(Diagnostics.begin(), Diagnostics.end());
        return FBuildResult(FFailureState{ std::move(Diagnostics) });
    }

    bool FBuildResult::IsSuccess() const
    {
        return std::holds_alternative<FSuccessState>(State);
    }

    bool FBuildResult::IsFailure() const
    {
        return std::holds_alternative<FFailureState>(State);
    }

    const FCandidate& FBuildResult::GetCandidate() const
    {
        if (!IsSuccess())
        {
            throw std::logic_error("Cannot get candidate from a failed build result");
        }
        return std::get<FSuccessState>(State).Candidate;
    }

    const FBuildResult::FDiagnosticList& FBuildResult::GetDiagnostics() const
    {
        if (!IsFailure())
        {
            throw std::logic_error("Cannot get diagnostics from a successful build result");
        }
        return std::get<FFailureState>(State).Diagnostics;
    }

    bool FBuildResult::operator==(const FBuildResult& Other) const
    {
        return State == Other.State;
    }

    bool FBuildResult::operator!=(const FBuildResult& Other) const
    {
        return !(*this == Other);
    }
}
