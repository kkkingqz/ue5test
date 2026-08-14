#pragma once

#include "GV2ContentCore/BuildResult.h"
#include "GV2ContentCore/RepositorySnapshot.h"

#include <cstdint>

// PCC-37: Application-scope atomic repository publication. Owns the
// "current" pinned GameDataRepository snapshot and its monotonic version.
// Publication only ever runs on the Game Thread; a same-hash candidate keeps
// the current handle/version unchanged, and a failed or non-publishable
// candidate never replaces Current. Old snapshots stay alive for as long as
// any FRepositoryReadHandle copy (session-pinned or otherwise) references
// them, per GameDataRepositoryContract.md "Snapshot identity"/"Reload".
class FGV2RepositoryPublisher final
{
public:
    // Publishes BuildResult as the new current repository if it is a valid,
    // publishable candidate. Returns true if Current now reflects a valid
    // repository (either because this candidate was just published, or
    // because it exactly matched the already-current content hash). Returns
    // false, leaving Current/GetVersion() unchanged, for a failed or
    // non-publishable candidate.
    bool PublishCandidate(GV2ContentCore::FBuildResult BuildResult);

    bool HasCurrent() const { return Current.IsValid(); }
    const GV2ContentCore::FRepositoryReadHandle& GetCurrent() const { return Current; }
    std::int64_t GetVersion() const { return Version; }

private:
    GV2ContentCore::FRepositoryReadHandle Current;
    std::int64_t Version = 0;
};
