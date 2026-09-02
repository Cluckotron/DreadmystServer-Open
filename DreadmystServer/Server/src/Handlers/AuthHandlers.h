// Authentication Packet Handlers
// Task 3.2: Account Creation Handler
// Task 3.3: Login Validation Handler

#pragma once

#include <string>

class Session;
class StlBuffer;

namespace Handlers
{

// Register all authentication-related packet handlers
void registerAuthHandlers();

// Individual handlers
void handleAuthenticate(Session& session, StlBuffer& data);

} // namespace Handlers
