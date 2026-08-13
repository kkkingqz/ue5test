#include "Bridge/GV2UiBindingRegistry.h"

void FGV2UiBindingRegistry::BeginSession(const int32 InSessionGeneration)
{
    check(IsInGameThread());
    check(InSessionGeneration > 0);

    SessionGeneration = InSessionGeneration;
    CurrentUiInstanceId.Reset();
    CurrentRevision = 0;
    NextHandleCounter = 1;
    Records.Reset();
}

void FGV2UiBindingRegistry::EndSession()
{
    check(IsInGameThread());

    SessionGeneration = 0;
    CurrentUiInstanceId.Reset();
    CurrentRevision = 0;
    NextHandleCounter = 1;
    Records.Reset();
}

bool FGV2UiBindingRegistry::PublishBindings(
    const FString& UiInstanceId,
    const int64 Revision,
    const TArray<FGV2UiBindingDefinition>& Definitions,
    TArray<FGV2UiBindingHandle>& OutHandles)
{
    FGV2PreparedBindingSet Candidate;
    if (!PrepareBindings(UiInstanceId, Revision, Definitions, Candidate))
    {
        OutHandles.Reset();
        return false;
    }
    OutHandles = Candidate.Handles;
    return CommitPreparedBindings(MoveTemp(Candidate));
}

bool FGV2UiBindingRegistry::PrepareBindings(
    const FString& UiInstanceId,
    const int64 Revision,
    const TArray<FGV2UiBindingDefinition>& Definitions,
    FGV2PreparedBindingSet& OutCandidate) const
{
    check(IsInGameThread());
    OutCandidate = {};

    int32 UiGeneration = 0;
    if (SessionGeneration <= 0
        || !TryReadTransientGeneration(UiInstanceId, TEXT("ui@"), UiGeneration)
        || UiGeneration != SessionGeneration
        || Revision <= 0
        || (UiInstanceId == CurrentUiInstanceId && Revision <= CurrentRevision))
    {
        return false;
    }

    TSet<FString> NodePathKeys;
    for (const FGV2UiBindingDefinition& Definition : Definitions)
    {
        const FString NodePathKey = MakeNodePathKey(Definition.NodeKeyPath);
        if (!ValidateDefinition(Definition) || NodePathKeys.Contains(NodePathKey))
        {
            return false;
        }
        NodePathKeys.Add(NodePathKey);
    }

    int64 CandidateCounter = NextHandleCounter;
    OutCandidate.Records.Reserve(Definitions.Num());
    OutCandidate.Handles.Reserve(Definitions.Num());

    for (const FGV2UiBindingDefinition& Definition : Definitions)
    {
        const FGV2UiBindingHandle Handle = FGV2UiBindingHandle::Create(FString::Printf(
            TEXT("runtime@%d:%lld"),
            SessionGeneration,
            CandidateCounter++));

        FGV2UiBindingRecord& Record = OutCandidate.Records.Add(Handle);
        Record.SessionGeneration = SessionGeneration;
        Record.UiInstanceId = UiInstanceId;
        Record.Revision = Revision;
        Record.NodeKeyPath = Definition.NodeKeyPath;
        Record.ElementId = Definition.ElementId;
        Record.CommandId = Definition.CommandId;
        Record.BoundArgs = Definition.BoundArgs;
        for (const FGV2UiInputFieldDefinition& InputField : Definition.InputFields)
        {
            Record.InputFieldTypes.Add(InputField.Name, InputField.Type);
            if (InputField.bRequired)
            {
                Record.RequiredInputFields.Add(InputField.Name);
            }
        }
        Record.InputSchemaId = Definition.InputSchemaId;
        OutCandidate.Handles.Add(Handle);
    }
    OutCandidate.UiInstanceId = UiInstanceId;
    OutCandidate.Revision = Revision;
    OutCandidate.NextHandleCounter = CandidateCounter;
    return true;
}

bool FGV2UiBindingRegistry::CommitPreparedBindings(FGV2PreparedBindingSet&& Candidate)
{
    check(IsInGameThread());
    if (Candidate.UiInstanceId.IsEmpty() || Candidate.Revision <= 0
        || Candidate.NextHandleCounter < NextHandleCounter)
    {
        return false;
    }
    Records = MoveTemp(Candidate.Records);
    CurrentUiInstanceId = MoveTemp(Candidate.UiInstanceId);
    CurrentRevision = Candidate.Revision;
    NextHandleCounter = Candidate.NextHandleCounter;
    return true;
}

EGV2BindingResolveResult FGV2UiBindingRegistry::Resolve(
    const FGV2UiBindingHandle& Handle,
    FGV2UiBindingRecord& OutRecord) const
{
    check(IsInGameThread());

    int32 HandleGeneration = 0;
    if (!TryReadTransientGeneration(Handle.ToString(), TEXT("runtime@"), HandleGeneration))
    {
        return EGV2BindingResolveResult::Invalid;
    }

    if (HandleGeneration != SessionGeneration)
    {
        return EGV2BindingResolveResult::Stale;
    }

    const FGV2UiBindingRecord* Record = Records.Find(Handle);
    if (Record == nullptr
        || Record->UiInstanceId != CurrentUiInstanceId
        || Record->Revision != CurrentRevision)
    {
        return EGV2BindingResolveResult::Invalid;
    }

    OutRecord = *Record;
    return EGV2BindingResolveResult::Found;
}

int32 FGV2UiBindingRegistry::Num() const
{
    return Records.Num();
}

int32 FGV2UiBindingRegistry::GetSessionGeneration() const
{
    return SessionGeneration;
}

const FString& FGV2UiBindingRegistry::GetUiInstanceId() const
{
    return CurrentUiInstanceId;
}

int64 FGV2UiBindingRegistry::GetRevision() const
{
    return CurrentRevision;
}

bool FGV2UiBindingRegistry::TryReadTransientGeneration(
    const FString& Value,
    const FString& Prefix,
    int32& OutGeneration)
{
    FString GenerationText;
    FString CounterText;
    if (!Value.StartsWith(Prefix)
        || !Value.Mid(Prefix.Len()).Split(TEXT(":"), &GenerationText, &CounterText)
        || GenerationText.IsEmpty()
        || CounterText.IsEmpty())
    {
        return false;
    }

    int64 Counter = 0;
    return LexTryParseString(OutGeneration, *GenerationText)
        && LexTryParseString(Counter, *CounterText)
        && OutGeneration > 0
        && Counter > 0;
}

bool FGV2UiBindingRegistry::ValidateDefinition(const FGV2UiBindingDefinition& Definition)
{
    if (Definition.NodeKeyPath.IsEmpty() || Definition.CommandId.IsEmpty())
    {
        return false;
    }

    for (const FString& Segment : Definition.NodeKeyPath)
    {
        if (Segment.IsEmpty())
        {
            return false;
        }
    }

    TSet<FName> FieldNames;
    for (const FGV2UiControlValue& BoundArg : Definition.BoundArgs)
    {
        if (BoundArg.Name.IsNone()
            || FieldNames.Contains(BoundArg.Name)
            || (BoundArg.Type == EGV2UiControlValueType::Number && !FMath::IsFinite(BoundArg.NumberValue)))
        {
            return false;
        }
        FieldNames.Add(BoundArg.Name);
    }

    for (const FGV2UiInputFieldDefinition& InputField : Definition.InputFields)
    {
        if (InputField.Name.IsNone() || FieldNames.Contains(InputField.Name))
        {
            return false;
        }
        FieldNames.Add(InputField.Name);
    }

    return true;
}

FString FGV2UiBindingRegistry::MakeNodePathKey(const TArray<FString>& NodeKeyPath)
{
    return FString::Join(NodeKeyPath, TEXT("\x1f"));
}
