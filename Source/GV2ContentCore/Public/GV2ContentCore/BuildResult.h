#pragma once

#include "GV2ContentCore/GV2ContentCore.h"
#include "GV2ContentCore/Diagnostic.h"
#include "GV2ContentCore/RepositorySnapshot.h"
#include "GV2ContentCore/Value.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <variant>
#include <vector>

namespace GV2ContentCore
{
    struct FPackageDescriptor;
    struct FBuildOptions;
    class FBuildResult;
    GV2_CONTENT_CORE_API FBuildResult BuildRepository(
        const std::vector<FPackageDescriptor>& PackageSet,
        const FBuildOptions& Options);

    enum class ECandidateStage : std::uint8_t
    {
        SchemaValidated,
        ReferencesValidated,
        RepositoryResolved,
    };

    /**
     * Immutable build artifact with explicit lifecycle stage. Only a
     * RepositoryResolved candidate may be handed to repository publication.
     */
    class GV2_CONTENT_CORE_API FCandidate final
    {
    public:
        FCandidate() = delete;
        explicit FCandidate(
            FValue InRootValue,
            const ECandidateStage InStage)
            : RootValue(std::move(InRootValue)), Stage(InStage) {}
        FCandidate(const FCandidate&) = default;
        FCandidate(FCandidate&&) noexcept = default;
        FCandidate& operator=(const FCandidate&) = delete;
        FCandidate& operator=(FCandidate&&) = delete;

        const FValue& GetRootValue() const
        {
            return Snapshot != nullptr ? Snapshot->GetRootValue() : RootValue;
        }
        ECandidateStage GetStage() const { return Stage; }
        bool IsPublishable() const
        {
            return Stage == ECandidateStage::RepositoryResolved && Snapshot != nullptr;
        }
        FRepositoryReadHandle GetReadHandle() const
        {
            if (Snapshot == nullptr) throw std::logic_error("Candidate has no resolved repository snapshot");
            return FRepositoryReadHandle(Snapshot);
        }

        bool operator==(const FCandidate& Other) const
        {
            return Stage == Other.Stage
                && GetRootValue() == Other.GetRootValue()
                && ((!Snapshot && !Other.Snapshot)
                    || (Snapshot && Other.Snapshot
                        && Snapshot->GetContentHash() == Other.Snapshot->GetContentHash()));
        }
        bool operator!=(const FCandidate& Other) const { return !(*this == Other); }

    private:
        friend FBuildResult BuildRepository(
            const std::vector<FPackageDescriptor>&,
            const FBuildOptions&);
        explicit FCandidate(std::shared_ptr<const FRepositorySnapshot> InSnapshot)
            : Stage(ECandidateStage::RepositoryResolved)
            , Snapshot(std::move(InSnapshot)) {}

        FValue RootValue;
        ECandidateStage Stage = ECandidateStage::SchemaValidated;
        std::shared_ptr<const FRepositorySnapshot> Snapshot;
    };

    /**
     * Result of a PortableContentCore build operation. Guaranteed by API design
     * to hold EITHER a complete stage-tagged artifact OR ordered diagnostics,
     * but never both simultaneously.
     */
    class GV2_CONTENT_CORE_API FBuildResult final
    {
    public:
        using FDiagnosticList = std::vector<FDiagnostic>;

        static FBuildResult Success(FCandidate Candidate);
        static FBuildResult Failure(FDiagnosticList Diagnostics);

        bool IsSuccess() const;
        bool IsFailure() const;

        const FCandidate& GetCandidate() const;
        const FDiagnosticList& GetDiagnostics() const;

        bool operator==(const FBuildResult& Other) const;
        bool operator!=(const FBuildResult& Other) const;

    private:
        struct FSuccessState
        {
            FCandidate Candidate;
            bool operator==(const FSuccessState& Other) const { return Candidate == Other.Candidate; }
        };

        struct FFailureState
        {
            FDiagnosticList Diagnostics;
            bool operator==(const FFailureState& Other) const { return Diagnostics == Other.Diagnostics; }
        };

        std::variant<FSuccessState, FFailureState> State;

        explicit FBuildResult(FSuccessState InState);
        explicit FBuildResult(FFailureState InState);
    };
}
