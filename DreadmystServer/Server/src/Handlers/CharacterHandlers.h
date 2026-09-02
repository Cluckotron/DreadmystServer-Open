// Character Packet Handlers
// Task 3.5: Character Creation Handler
// Task 3.6: Character List Handler
// Task 3.7: Character Deletion Handler

#pragma once

class Session;
class StlBuffer;

namespace Handlers
{

// Register all character-related packet handlers
void registerCharacterHandlers();

// Individual handlers
void handleCharacterList(Session& session, StlBuffer& data);
void handleCharCreate(Session& session, StlBuffer& data);
void handleDeleteCharacter(Session& session, StlBuffer& data);

// Helper: Send character list to client
void sendCharacterList(Session& session);

} // namespace Handlers
