// Packet Sender - Helper functions for sending common packets
// Task 2.7: Error Packet Responses

#include "stdafx.h"
#include "Network/PacketSender.h"
#include "Network/Session.h"
#include "Core/Logger.h"
#include "GamePacketBase.h"
#include "GamePacketServer.h"

namespace PacketSender
{

void sendAuthResult(Session& session, AccountDefines::AuthenticateResult result)
{
    GP_Server_Validate packet;
    packet.m_result = static_cast<uint8_t>(result);
    packet.m_serverTime = std::time(nullptr);

    StlBuffer data = buildPacket(packet.getOpcode());
    packet.pack(data);
    session.sendPacket(data);

    LOG_DEBUG("Session %u: Sent auth result %u",
              session.getId(), static_cast<uint8_t>(result));
}

void sendQueuePosition(Session& session, int32_t position)
{
    GP_Server_QueuePosition packet;
    packet.m_position = position;

    StlBuffer data = buildPacket(packet.getOpcode());
    packet.pack(data);
    session.sendPacket(data);

    LOG_DEBUG("Session %u: Sent queue position %d", session.getId(), position);
}

void sendWorldError(Session& session, uint8_t errorCode, const std::string& message)
{
    GP_Server_WorldError packet;
    packet.m_code = errorCode;
    packet.m_message = message;

    StlBuffer data = buildPacket(packet.getOpcode());
    packet.pack(data);
    session.sendPacket(data);

    LOG_DEBUG("Session %u: Sent world error %u: %s",
              session.getId(), errorCode, message.empty() ? "(no message)" : message.c_str());
}

void sendPacket(Session& session, uint16_t opcode, const StlBuffer& data)
{
    StlBuffer packet = buildPacket(opcode, data);
    session.sendPacket(packet);
}

StlBuffer buildPacket(uint16_t opcode)
{
    StlBuffer buf;
    buf << opcode;
    return buf;
}

StlBuffer buildPacket(uint16_t opcode, const StlBuffer& data)
{
    StlBuffer buf;
    buf << opcode;
    // Copy data byte by byte (StlBuffer doesn't have append)
    for (size_t i = 0; i < data.size(); ++i) {
        buf << data.data()[i];
    }
    return buf;
}

} // namespace PacketSender
