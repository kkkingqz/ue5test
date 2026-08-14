#include "Application/GV2RepositoryPublisher.h"

#include "CoreMinimal.h"
#include "Templates/UnrealTemplate.h"

bool FGV2RepositoryPublisher::PublishCandidate(GV2ContentCore::FBuildResult BuildResult)
{
    check(IsInGameThread());

    if (BuildResult.IsFailure())
    {
        return false;
    }

    const GV2ContentCore::FCandidate& Candidate = BuildResult.GetCandidate();
    if (!Candidate.IsPublishable())
    {
        return false;
    }

    GV2ContentCore::FRepositoryReadHandle CandidateHandle = Candidate.GetReadHandle();
    if (Current.IsValid() && Current.GetContentHash() == CandidateHandle.GetContentHash())
    {
        // Same-hash candidate: already current, do not bump the version.
        return true;
    }

    Current = MoveTemp(CandidateHandle);
    ++Version;
    return true;
}
