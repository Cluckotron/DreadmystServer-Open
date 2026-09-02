// Character Packet Handlers
// Task 3.5: Character Creation Handler
// Task 3.6: Character List Handler
// Task 3.7: Character Deletion Handler

#include "stdafx.h"
#include "Handlers/CharacterHandlers.h"
#include "Network/Session.h"
#include "Network/PacketRouter.h"
#include "Database/CharacterDb.h"
#include "Core/Logger.h"
#include "GamePacketBase.h"
#include "GamePacketClient.h"
#include "GamePacketServer.h"
#include "CharacterDefines.h"

namespace Handlers
{

// ============================================================================
// Helper Functions
// ============================================================================

void sendCharacterList(Session& session)
{
    auto characters = CharacterDb::getCharactersByAccount(session.getAccountId());

    GP_Server_CharacterList response;
    response.m_characters.reserve(characters.size());

    for (const auto& ch : characters)
    {
        GP_Server_CharacterList::Character charData;
        charData.guid = static_cast<uint32_t>(ch.guid);
        charData.name = ch.name;
        charData.classId = static_cast<uint8_t>(ch.classId);
        charData.gender = static_cast<uint8_t>(ch.gender);
        charData.level = ch.level;
        charData.portrait = ch.portraitId;
        response.m_characters.push_back(charData);
    }

    StlBuffer packet;
    uint16_t opcode = response.getOpcode();
    packet << opcode;
    response.pack(packet);

    session.sendPacket(packet);

    LOG_DEBUG("Session %u: Sent character list (%zu characters)",
              session.getId(), characters.size());
}

static void sendCharCreateResult(Session& session, CharacterDefines::NameError result)
{
    GP_Server_CharaCreateResult response;
    response.m_result = static_cast<uint8_t>(result);

    StlBuffer packet;
    uint16_t opcode = response.getOpcode();
    packet << opcode;
    response.pack(packet);

    session.sendPacket(packet);
}

// ============================================================================
// Handlers
// ============================================================================

void handleCharacterList(Session& session, StlBuffer& data)
{
    (void)data;  // CharacterList request has no data

    LOG_DEBUG("Session %u: Character list requested for account %d",
              session.getId(), session.getAccountId());

    sendCharacterList(session);
}

void handleCharCreate(Session& session, StlBuffer& data)
{
    // Parse the incoming packet
    GP_Client_CharCreate packet;
    packet.unpack(data);

    LOG_INFO("Session %u: Character creation request - name='%s' class=%d gender=%d portrait=%d",
             session.getId(), packet.m_name.c_str(), packet.m_classId, packet.m_gender, packet.m_portrait);

    // Validate name format
    if (!CharacterDb::isValidName(packet.m_name))
    {
        // Determine specific error
        CharacterDefines::NameError error = CharacterDefines::NameError::InvalidChars;

        if (packet.m_name.length() < 2)
        {
            error = CharacterDefines::NameError::TooShort;
        }
        else if (packet.m_name.length() > 16)
        {
            error = CharacterDefines::NameError::TooLong;
        }

        LOG_INFO("Session %u: Character name invalid: %s",
                 session.getId(), packet.m_name.c_str());
        sendCharCreateResult(session, error);
        return;
    }

    // Check if name is taken
    if (CharacterDb::isNameTaken(packet.m_name))
    {
        LOG_INFO("Session %u: Character name already taken: %s",
                 session.getId(), packet.m_name.c_str());
        sendCharCreateResult(session, CharacterDefines::NameError::AlreadyExists);
        return;
    }

    // Check max characters per account
    int32_t currentCount = CharacterDb::getCharacterCount(session.getAccountId());
    if (currentCount >= CharacterDb::MAX_CHARACTERS_PER_ACCOUNT)
    {
        LOG_INFO("Session %u: Account %d at max characters (%d)",
                 session.getId(), session.getAccountId(), CharacterDb::MAX_CHARACTERS_PER_ACCOUNT);
        // No specific error code for this - use Reserved as a generic "can't create"
        sendCharCreateResult(session, CharacterDefines::NameError::Reserved);
        return;
    }

    // Create the character
    auto newGuid = CharacterDb::createCharacter(
        session.getAccountId(),
        packet.m_name,
        packet.m_classId,
        packet.m_gender,
        packet.m_portrait
    );

    if (!newGuid.has_value())
    {
        LOG_ERROR("Session %u: Failed to create character: %s",
                  session.getId(), packet.m_name.c_str());
        sendCharCreateResult(session, CharacterDefines::NameError::Reserved);
        return;
    }

    LOG_INFO("Session %u: Created character '%s' (GUID %d) for account %d",
             session.getId(), packet.m_name.c_str(), *newGuid, session.getAccountId());

    // Send success
    sendCharCreateResult(session, CharacterDefines::NameError::Success);

    // Send updated character list
    sendCharacterList(session);
}

void handleDeleteCharacter(Session& session, StlBuffer& data)
{
    // Parse the incoming packet
    GP_Client_DeleteCharacter packet;
    packet.unpack(data);

    LOG_INFO("Session %u: Delete character request - GUID %u",
             session.getId(), packet.m_playerGuid);

    // Attempt to delete (CharacterDb verifies ownership)
    bool success = CharacterDb::deleteCharacter(
        static_cast<int32_t>(packet.m_playerGuid),
        session.getAccountId()
    );

    if (success)
    {
        LOG_INFO("Session %u: Deleted character GUID %u", session.getId(), packet.m_playerGuid);
    }
    else
    {
        LOG_WARN("Session %u: Failed to delete character GUID %u (not found or not owned)",
                 session.getId(), packet.m_playerGuid);
    }

    // Always send updated character list (whether or not deletion succeeded)
    sendCharacterList(session);
}

void registerCharacterHandlers()
{
    // Character list - must be authenticated
    sPacketRouter.registerHandler(
        Opcode::Client_CharacterList,
        handleCharacterList,
        SessionState::Authenticated,
        true,  // Allow higher states (could request while in world)
        "Client_CharacterList"
    );

    // Character creation - must be authenticated
    sPacketRouter.registerHandler(
        Opcode::Client_CharCreate,
        handleCharCreate,
        SessionState::Authenticated,
        false,  // Only in character select screen
        "Client_CharCreate"
    );

    // Character deletion - must be authenticated
    sPacketRouter.registerHandler(
        Opcode::Client_DeleteCharacter,
        handleDeleteCharacter,
        SessionState::Authenticated,
        false,  // Only in character select screen
        "Client_DeleteCharacter"
    );

    LOG_DEBUG("Character handlers registered");
}

} // namespace Handlers
