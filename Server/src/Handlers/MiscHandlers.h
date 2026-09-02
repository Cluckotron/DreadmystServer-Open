// Miscellaneous Packet Handlers
// Task 2.6: Ping/Keepalive Handling

#pragma once

class Session;
class StlBuffer;

namespace Handlers
{
    // Handle ping packet - echoes back to client
    void handlePing(Session& session, StlBuffer& data);

    // Register all miscellaneous handlers
    void registerMiscHandlers();
}
