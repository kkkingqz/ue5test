#pragma once

#include "GV2ContentAuthoring/AuthoringTypes.h"
#include "GV2ContentAuthoring/GV2ContentAuthoring.h"

namespace GV2ContentAuthoring
{

class GV2_CONTENT_AUTHORING_API FAuthoringService
{
public:
    static FAuthoringResult CreateDefinition(const FCreateDefinitionParams& Params);
    static FAuthoringResult SetField(const FSetFieldParams& Params);
    static FAuthoringResult BatchSetFields(const FBatchSetFieldsParams& Params);
    static FAuthoringResult ApplyOperations(const FApplyOperationsParams& Params);
    static FAuthoringResult DeleteDefinition(const FDeleteDefinitionParams& Params);
    static FAuthoringResult RenameDefinition(const FRenameDefinitionParams& Params);
    static FAuthoringResult DuplicateDefinition(const FDuplicateDefinitionParams& Params);

    static FFileStateStamp GetFileStateStamp(const std::filesystem::path& FilePath);
    static FFileStateStamp ComputeStamp(std::string_view Content);
};

} // namespace GV2ContentAuthoring
