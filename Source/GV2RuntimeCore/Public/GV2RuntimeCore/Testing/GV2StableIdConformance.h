#pragma once

#include "GV2RuntimeCore/GV2StableId.h"

#include <array>
#include <string>
#include <string_view>

namespace GV2RuntimeCore::Testing
{
struct FStableIdCase
{
    std::string_view Value;
    bool bValid;
    EStableIdError Error;
};

inline constexpr std::array StableIdCases{
    FStableIdCase{"core:item.weapon.iron_sword", true, EStableIdError::None},
    FStableIdCase{"weather_mod:item.ring.storm", true, EStableIdError::None},
    FStableIdCase{"a:a.a", true, EStableIdError::None},
    FStableIdCase{"", false, EStableIdError::Empty},
    FStableIdCase{"Core:item.weapon", false, EStableIdError::InvalidSegmentStart},
    FStableIdCase{"core:Item.weapon", false, EStableIdError::InvalidSegmentStart},
    FStableIdCase{"core:item.Weapon", false, EStableIdError::InvalidSegmentStart},
    FStableIdCase{"core:item.weapon-name", false, EStableIdError::InvalidCharacter},
    FStableIdCase{"core:item.weapon name", false, EStableIdError::InvalidCharacter},
    FStableIdCase{"core:item.weap\xC3\xB6n", false, EStableIdError::NonAscii},
    FStableIdCase{"1core:item.weapon", false, EStableIdError::InvalidSegmentStart},
    FStableIdCase{"core:1item.weapon", false, EStableIdError::InvalidSegmentStart},
    FStableIdCase{"core:item.1weapon", false, EStableIdError::InvalidSegmentStart},
    FStableIdCase{"coreitem.weapon", false, EStableIdError::InvalidSeparator},
    FStableIdCase{"core::item.weapon", false, EStableIdError::InvalidSeparator},
    FStableIdCase{"core:item", false, EStableIdError::InvalidSeparator},
    FStableIdCase{"core:.weapon", false, EStableIdError::EmptySegment},
    FStableIdCase{"core:item.", false, EStableIdError::EmptySegment},
    FStableIdCase{"core:item.weapon..sword", false, EStableIdError::EmptySegment},
};

inline std::string RunStableIdConformance()
{
    for (std::size_t Index = 0; Index < StableIdCases.size(); ++Index)
    {
        const FStableIdCase& Case = StableIdCases[Index];
        EStableIdError Error = EStableIdError::None;
        FStableIdView Parsed;
        const bool bValid = FStableId::Parse(Case.Value, Parsed, &Error);
        if (bValid != Case.bValid || Error != Case.Error)
        {
            return "stable_id_case_" + std::to_string(Index);
        }
    }

    const std::string MaxSegment(64, 'a');
    EStableIdError Error = EStableIdError::None;
    if (!FStableId::IsValidSegment(MaxSegment, &Error)
        || Error != EStableIdError::None
        || FStableId::IsValidSegment(MaxSegment + "a", &Error)
        || Error != EStableIdError::TooLong)
    {
        return "segment_length_boundary";
    }

    const std::string MaxId = std::string(64, 'n') + ":" + std::string(64, 'k')
        + "." + std::string(62, 'p');
    if (MaxId.size() != FStableId::MaxLength
        || !FStableId::IsValid(MaxId, &Error)
        || FStableId::IsValid(MaxId + "a", &Error)
        || Error != EStableIdError::TooLong)
    {
        return "stable_id_length_boundary";
    }

    FStableIdView Parsed;
    if (!FStableId::Parse("core:command.location.travel", Parsed, &Error)
        || Parsed.Namespace != "core"
        || Parsed.Kind != "command"
        || Parsed.Path != "location.travel")
    {
        return "parsed_components";
    }
    struct FInstanceIdCase
    {
        std::string_view Value;
        bool bValid;
        EStableIdError Error;
    };

    const std::array InstanceIdCases{
        FInstanceIdCase{"item@1", true, EStableIdError::None},
        FInstanceIdCase{"item@42", true, EStableIdError::None},
        FInstanceIdCase{"actor@7", true, EStableIdError::None},
        FInstanceIdCase{"quest@100", true, EStableIdError::None},
        FInstanceIdCase{"", false, EStableIdError::Empty},
        FInstanceIdCase{"item@0", false, EStableIdError::InvalidInstanceId},
        FInstanceIdCase{"item@01", false, EStableIdError::InvalidInstanceId},
        FInstanceIdCase{"item@-1", false, EStableIdError::InvalidInstanceId},
        FInstanceIdCase{"Item@1", false, EStableIdError::InvalidSegmentStart},
        FInstanceIdCase{"item1", false, EStableIdError::InvalidSeparator},
        FInstanceIdCase{"item@@1", false, EStableIdError::InvalidSeparator},
        FInstanceIdCase{"item@abc", false, EStableIdError::InvalidInstanceId},
        FInstanceIdCase{"@1", false, EStableIdError::EmptySegment},
    };

    for (std::size_t Index = 0; Index < InstanceIdCases.size(); ++Index)
    {
        const FInstanceIdCase& Case = InstanceIdCases[Index];
        EStableIdError InstError = EStableIdError::None;
        FInstanceIdView ParsedInst;
        const bool bValid = FStableId::ParseInstanceId(Case.Value, ParsedInst, &InstError);
        if (bValid != Case.bValid || InstError != Case.Error)
        {
            return "instance_id_case_" + std::to_string(Index);
        }
    }

    return {};
}
}
