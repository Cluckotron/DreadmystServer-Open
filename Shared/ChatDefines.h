#pragma once

#include <stdint.h>

namespace ChatDefines
{
    // Chat channel types
    enum class Channels : uint8_t
    {
        Say = 0,          // Local say
        Yell = 1,         // Yell (wider range)
        Whisper = 2,      // Private message
        Party = 3,        // Party chat
        Guild = 4,        // Guild chat
        AllChat = 5,      // Global chat
        System = 6,       // System messages
        SystemCenter = 7, // Centered system messages
        ExpPurple = 8,    // Experience notifications
        RedWarning = 9,   // Red warning messages
    };
}
