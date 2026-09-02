#pragma once

#include <stdint.h>
#include <string>

namespace GuildDefines
{
    // Guild member ranks
    enum class Rank : uint8_t
    {
        Initiate = 0,
        Member = 1,
        Officer = 2,
        Leader = 3,
    };

    // Guild name validation errors
    enum class NameError : uint8_t
    {
        Success = 0,
        TooShort = 1,
        TooLong = 2,
        InvalidCharacters = 3,
        ProfanityFilter = 4,
        AlreadyExists = 5,
    };

    // Maximum guild members
    constexpr int MaxMembers = 100;
    constexpr int MinGuildNameLength = 3;
    constexpr int MaxGuildNameLength = 24;

    inline Rank operator+(Rank rank, int delta)
    {
        return static_cast<Rank>(static_cast<int>(rank) + delta);
    }

    inline Rank operator-(Rank rank, int delta)
    {
        return static_cast<Rank>(static_cast<int>(rank) - delta);
    }
}

namespace GuildFunctions
{
    // Validate guild name and return error code
    inline GuildDefines::NameError invalidNameError(const std::string& name)
    {
        if (name.length() < GuildDefines::MinGuildNameLength)
            return GuildDefines::NameError::TooShort;
        if (name.length() > GuildDefines::MaxGuildNameLength)
            return GuildDefines::NameError::TooLong;

        // Check for valid characters (alphanumeric and spaces only)
        for (char c : name)
        {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != ' ')
                return GuildDefines::NameError::InvalidCharacters;
        }

        return GuildDefines::NameError::Success;
    }

    // Get human-readable error message
    inline std::string nameErrorStr(GuildDefines::NameError error)
    {
        switch (error)
        {
            case GuildDefines::NameError::Success: return "";
            case GuildDefines::NameError::TooShort: return "Guild name is too short.";
            case GuildDefines::NameError::TooLong: return "Guild name is too long.";
            case GuildDefines::NameError::InvalidCharacters: return "Guild name contains invalid characters.";
            case GuildDefines::NameError::ProfanityFilter: return "Guild name is not allowed.";
            case GuildDefines::NameError::AlreadyExists: return "A guild with that name already exists.";
        }
        return "Unknown error.";
    }

    inline std::string rankName(GuildDefines::Rank rank)
    {
        switch (rank)
        {
            case GuildDefines::Rank::Initiate: return "Initiate";
            case GuildDefines::Rank::Member: return "Member";
            case GuildDefines::Rank::Officer: return "Officer";
            case GuildDefines::Rank::Leader: return "Leader";
        }
        return "Unknown";
    }

    inline bool hasOfficerPowerOver(GuildDefines::Rank myRank, GuildDefines::Rank otherRank)
    {
        return myRank >= GuildDefines::Rank::Officer && myRank > otherRank;
    }
}
