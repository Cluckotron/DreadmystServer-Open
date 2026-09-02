// Packet Router Framework - Routes packets to handlers with state validation
// Task 2.5: Packet Dispatch System

#pragma once

#include <functional>
#include <unordered_map>
#include <cstdint>
#include <string>
#include "Network/Session.h"

class StlBuffer;

using PacketHandler = std::function<void(Session&, StlBuffer&)>;

// Information about a registered packet handler
struct PacketHandlerInfo
{
    PacketHandler handler;
    SessionState requiredState = SessionState::Connected;
    bool allowHigherStates = true;  // Allow if session is in a "higher" state
    std::string name;               // Handler name for debugging
};

// Get human-readable name for an opcode
const char* getOpcodeName(uint16_t opcode);

class PacketRouter
{
public:
    static PacketRouter& instance();

    // Register a handler with state requirements
    void registerHandler(
        uint16_t opcode,
        PacketHandler handler,
        SessionState requiredState = SessionState::Connected,
        bool allowHigherStates = true,
        const char* name = nullptr
    );

    // Dispatch a packet to its handler (validates state)
    void dispatch(Session& session, uint16_t opcode, StlBuffer& data);

    // Initialize all handlers
    void initialize();

    // Check if handler exists
    bool hasHandler(uint16_t opcode) const;

    // Get handler count
    size_t getHandlerCount() const { return m_handlers.size(); }

private:
    PacketRouter() = default;

    // Check if session state is valid for this handler
    bool checkState(const Session& session, const PacketHandlerInfo& info) const;

    // Get state "level" for comparison
    int getStateLevel(SessionState state) const;

    std::unordered_map<uint16_t, PacketHandlerInfo> m_handlers;
};

#define sPacketRouter PacketRouter::instance()
