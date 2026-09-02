// Packet Sender - Helper functions for sending common packets
// Task 2.7: Error Packet Responses

#pragma once

#include <string>
#include <cstdint>
#include "AccountDefines.h"

class Session;
class StlBuffer;

namespace PacketSender
{
    // Authentication responses
    void sendAuthResult(Session& session, AccountDefines::AuthenticateResult result);
    void sendQueuePosition(Session& session, int32_t position);

    // World errors
    void sendWorldError(Session& session, uint8_t errorCode, const std::string& message = "");

    // Generic packet sending (builds header automatically)
    void sendPacket(Session& session, uint16_t opcode, const StlBuffer& data);

    // Build a packet with opcode header
    StlBuffer buildPacket(uint16_t opcode);
    StlBuffer buildPacket(uint16_t opcode, const StlBuffer& data);
}

// Common world error codes (can be extended)
namespace WorldError
{
    constexpr uint8_t None = 0;
    constexpr uint8_t GenericError = 1;
    constexpr uint8_t NotEnoughGold = 2;
    constexpr uint8_t InventoryFull = 3;
    constexpr uint8_t ItemNotFound = 4;
    constexpr uint8_t InvalidTarget = 5;
    constexpr uint8_t OutOfRange = 6;
    constexpr uint8_t NotEnoughMana = 7;
    constexpr uint8_t SpellOnCooldown = 8;
    constexpr uint8_t CannotDoThat = 9;
    constexpr uint8_t LevelTooLow = 10;
    constexpr uint8_t QuestNotAvailable = 11;
    constexpr uint8_t AlreadyInGuild = 12;
    constexpr uint8_t NotInGuild = 13;
    constexpr uint8_t NotGuildLeader = 14;
    constexpr uint8_t PlayerNotFound = 15;
    constexpr uint8_t AlreadyInParty = 16;
    constexpr uint8_t NotInParty = 17;
    constexpr uint8_t PartyFull = 18;
    constexpr uint8_t TradeError = 19;
    constexpr uint8_t BankError = 20;
}
