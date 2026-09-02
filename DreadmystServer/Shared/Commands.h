#pragma once

#include <string>
#include <cstring>

class Commands;

// Command table entry structure for console commands
struct CCommand
{
    const char* name;                                           // Command name
    bool (*handler)(const char* args, Commands* thisptr);       // Handler function
    CCommand* childCommands;                                    // Subcommands (nullptr if none)
};

// Base class for command processing
// ConsoleWindow inherits from this to provide command handling
class Commands
{
public:
    virtual ~Commands() = default;

    // Output methods - override in derived class to handle messages
    virtual void error(const std::string& txt) = 0;
    virtual void warning(const std::string& txt) = 0;

protected:
    // Execute a command from the command table
    // Returns true if command was found and executed
    bool executeCommand(CCommand* table, const std::string& text)
    {
        if (!table || text.empty())
            return false;

        // Parse command name and arguments
        std::string cmd;
        std::string args;

        size_t spacePos = text.find(' ');
        if (spacePos != std::string::npos)
        {
            cmd = text.substr(0, spacePos);
            args = text.substr(spacePos + 1);
        }
        else
        {
            cmd = text;
        }

        // Search for command in table
        for (CCommand* entry = table; entry->name != nullptr; ++entry)
        {
            if (cmd == entry->name)
            {
                // If this command has child commands, try them first
                if (entry->childCommands && !args.empty())
                {
                    if (executeCommand(entry->childCommands, args))
                        return true;
                }

                // Execute the handler if present
                if (entry->handler)
                    return entry->handler(args.c_str(), this);

                return false;
            }
        }

        return false;
    }
};
