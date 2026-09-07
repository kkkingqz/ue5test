#include "UI/GV2PresentationAuthorityProbe.h"

#if !UE_BUILD_SHIPPING

#include "HAL/PlatformAtomics.h"

namespace GV2PresentationAuthorityProbe
{
namespace
{
// Monotonic and never reset: a test reads it before and after the call it
// brackets and compares the delta, so a shared absolute value across tests is
// harmless and avoids any ordering dependency between them.
volatile int64 GResolveCount = 0;
}

uint64 GetResolveCount()
{
    return static_cast<uint64>(FPlatformAtomics::AtomicRead(&GResolveCount));
}

void NoteResolve()
{
    FPlatformAtomics::InterlockedIncrement(&GResolveCount);
}
}

#endif
