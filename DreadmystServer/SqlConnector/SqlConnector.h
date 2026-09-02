// SqlConnector.h - SQLite database wrapper with factory pattern for Dreadmyst client
#pragma once

#include "QueryResult.h"
#include <memory>
#include <string>

// Database type enumeration
enum class SqlType
{
    SQLite = 0,
    MySQL = 1,  // Not implemented, for future use
};

// SqlConnector - wrapper around SqliteConnection with factory pattern
class SqlConnector
{
public:
    SqlConnector() = default;
    ~SqlConnector() = default;

    // Factory method: creates a new database connection
    // name is typically the database identifier, path is the file path
    static std::shared_ptr<SqlConnector> create(SqlType type, const std::string& name, const std::string& path)
    {
        (void)type;  // Currently only SQLite is supported
        (void)name;  // Name is informational

        auto connector = std::make_shared<SqlConnector>();
        if (!connector->m_connection.open(path))
        {
            return nullptr;
        }
        return connector;
    }

    // Execute a query and return the result
    std::shared_ptr<QueryResult> query(const std::string& sql)
    {
        auto result = std::make_shared<QueryResult>();

        sqlite3_stmt* stmt = nullptr;
        if (m_connection.isOpen())
        {
            // Get raw db handle - we need to access it through a method
            sqlite3* db = m_connection.getDb();
            if (db && sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK)
            {
                result->setStatement(stmt);
                result->buildIndex(0);  // Build index for convenient access
            }
        }
        return result;
    }

    // Execute a statement without returning results
    bool execute(const std::string& sql)
    {
        return m_connection.execute(sql);
    }

    // Check if connection is open
    bool isOpen() const { return m_connection.isOpen(); }

private:
    SqliteConnection m_connection;
};
