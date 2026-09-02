// Database Manager - SQLite wrapper singleton
// Task 2.1: Database Manager Setup

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <cstdint>
#include <sqlite3.h>

// Forward declaration
class PreparedStatement;

// Represents a single row of query results
class QueryRow
{
public:
    QueryRow(const std::vector<std::string>& columns, const std::vector<std::string>& values)
        : m_columns(columns), m_values(values) {}

    // Get value by column index
    const std::string& operator[](size_t index) const { return m_values[index]; }

    // Get value by column name
    std::string get(const std::string& column) const;

    // Type-safe getters
    int getInt(const std::string& column) const;
    int64_t getInt64(const std::string& column) const;
    double getDouble(const std::string& column) const;
    std::string getString(const std::string& column) const;
    bool isNull(const std::string& column) const;

    size_t columnCount() const { return m_columns.size(); }
    const std::string& columnName(size_t index) const { return m_columns[index]; }

private:
    std::vector<std::string> m_columns;
    std::vector<std::string> m_values;
};

// Represents a complete query result set
class QueryResult
{
public:
    QueryResult() = default;
    QueryResult(bool success, const std::string& error = "")
        : m_success(success), m_error(error) {}

    bool success() const { return m_success; }
    const std::string& error() const { return m_error; }

    bool empty() const { return m_rows.empty(); }
    size_t rowCount() const { return m_rows.size(); }

    void addRow(const QueryRow& row) { m_rows.push_back(row); }

    const QueryRow& operator[](size_t index) const { return m_rows[index]; }

    // Iterator support
    std::vector<QueryRow>::const_iterator begin() const { return m_rows.begin(); }
    std::vector<QueryRow>::const_iterator end() const { return m_rows.end(); }

private:
    bool m_success = false;
    std::string m_error;
    std::vector<QueryRow> m_rows;
};

// RAII wrapper for prepared statements
class PreparedStatement
{
public:
    PreparedStatement() = default;
    PreparedStatement(sqlite3_stmt* stmt) : m_stmt(stmt) {}
    ~PreparedStatement();

    // Non-copyable
    PreparedStatement(const PreparedStatement&) = delete;
    PreparedStatement& operator=(const PreparedStatement&) = delete;

    // Movable
    PreparedStatement(PreparedStatement&& other) noexcept;
    PreparedStatement& operator=(PreparedStatement&& other) noexcept;

    bool valid() const { return m_stmt != nullptr; }

    // Bind parameters (1-indexed like SQLite)
    void bind(int index, int value);
    void bind(int index, int64_t value);
    void bind(int index, double value);
    void bind(int index, const std::string& value);
    void bindNull(int index);

    // Execute and get results
    bool step();     // SELECT iterator: true only when SQLITE_ROW is available
    bool execute();  // DML/DDL: true only when sqlite3_step returns SQLITE_DONE
    void reset();    // Reset for re-use with new bindings

    // Get column values (0-indexed)
    int getInt(int column) const;
    int64_t getInt64(int column) const;
    double getDouble(int column) const;
    std::string getString(int column) const;
    bool isNull(int column) const;

    int columnCount() const;
    std::string columnName(int column) const;

private:
    sqlite3_stmt* m_stmt = nullptr;
};

// Main database manager singleton
class DatabaseManager
{
public:
    static DatabaseManager& instance();

    // Database lifecycle
    bool open(const std::string& path);
    void close();
    bool isOpen() const { return m_db != nullptr; }

    // Query execution
    QueryResult query(const std::string& sql);
    bool execute(const std::string& sql);
    bool executeFile(const std::string& path);

    // Prepared statements
    PreparedStatement prepare(const std::string& sql);

    // Transactions
    void beginTransaction();
    void commit();
    void rollback();

    // Utility
    int64_t lastInsertId() const;
    int changesCount() const;
    const std::string& lastError() const { return m_lastError; }

    // Get raw handle (use sparingly)
    sqlite3* handle() const { return m_db; }

private:
    DatabaseManager() = default;
    ~DatabaseManager();

    // Non-copyable
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    // Internal execute - must be called with mutex held
    bool executeInternal(const std::string& sql);

    sqlite3* m_db = nullptr;
    std::string m_lastError;
    mutable std::mutex m_mutex;
};

#define sDatabase DatabaseManager::instance()
