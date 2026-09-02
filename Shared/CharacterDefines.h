#pragma once

#include <stdint.h>
#include <string>

namespace CharacterDefines
{
    // Character name validation errors
    enum class NameError : uint8_t
    {
        Success = 0,
        TooShort = 1,
        TooLong = 2,
        InvalidChars = 3,
        AlreadyExists = 4,
        Reserved = 5,
        Profanity = 6,
    };

    // Name constraints
    constexpr int MinNameLength = 3;
    constexpr int MaxNameLength = 12;

    // Nested namespace for name-related constants
    namespace Names
    {
        constexpr int MaxLength = MaxNameLength;
        constexpr int MinLength = MinNameLength;
    }

    // Nested namespace for portrait-related constants
    namespace Portraits
    {
        constexpr int NumMale = 10;
        constexpr int NumFemale = 10;
    }
}

// Helper functions
namespace CharacterFunctions
{
    inline std::string nameErrorStr(CharacterDefines::NameError error)
    {
        switch (error)
        {
            case CharacterDefines::NameError::Success: return "Success";
            case CharacterDefines::NameError::TooShort: return "Name is too short";
            case CharacterDefines::NameError::TooLong: return "Name is too long";
            case CharacterDefines::NameError::InvalidChars: return "Name contains invalid characters";
            case CharacterDefines::NameError::AlreadyExists: return "Name already exists";
            case CharacterDefines::NameError::Reserved: return "Name is reserved";
            case CharacterDefines::NameError::Profanity: return "Name contains inappropriate content";
            default: return "Unknown error";
        }
    }

    // Validate character name, returns nullptr if valid, error string if invalid
    inline const char* invalidNameError(const std::string& name)
    {
        if (name.length() < CharacterDefines::MinNameLength)
            return "Name is too short";
        if (name.length() > CharacterDefines::MaxNameLength)
            return "Name is too long";
        // Check for valid characters (letters only, first letter uppercase)
        for (size_t i = 0; i < name.length(); ++i)
        {
            char c = name[i];
            if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
                return "Name contains invalid characters";
        }
        return nullptr;
    }

    // Format name with first letter uppercase, rest lowercase
    inline std::string formatName(const std::string& name)
    {
        if (name.empty()) return name;
        std::string result = name;
        result[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[0])));
        for (size_t i = 1; i < result.length(); ++i)
            result[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[i])));
        return result;
    }
}
