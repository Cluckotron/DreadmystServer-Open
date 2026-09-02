// Database Manager - SQLite wrapper implementation
// Task 2.1: Database Manager Setup

#include "stdafx.h"
#include "Database/DatabaseManager.h"
#include "Core/Logger.h"
#include <fstream>
#include <sstream>

// ============================================================================
// QueryRow Implementation
// ============================================================================

std::string QueryRow::get(const std::string& column) const
{
    for (size_t i = 0; i < m_columns.size(); ++i) {
        if (m_columns[i] == column) {
            return m_values[i];
        }
    }
    return "";
}

int QueryRow::getInt(const std::string& column) const
{
    std::string val = get(column);
    return val.empty() ? 0 : std::stoi(val);
}

int64_t QueryRow::getInt64(const std::string& column) const
{
    std::string val = get(column);
    return val.empty() ? 0 : std::stoll(val);
}

double QueryRow::getDouble(const std::string& column) const
{
    std::string val = get(column);
    return val.empty() ? 0.0 : std::stod(val);
}

std::string QueryRow::getString(const std::string& column) const
{
    return get(column);
}

bool QueryRow::isNull(const std::string& column) const
{
    for (size_t i = 0; i < m_columns.size(); ++i) {
        if (m_columns[i] == column) {
            return m_values[i].empty();
        }
    }
    return true;
}

// ============================================================================
// PreparedStatement Implementation
// ============================================================================

PreparedStatement::~PreparedStatement()
{
    if (m_stmt) {
        sqlite3_finalize(m_stmt);
        m_stmt = nullptr;
    }
}

PreparedStatement::PreparedStatement(PreparedStatement&& other) noexcept
    : m_stmt(other.m_stmt)
{
    other.m_stmt = nullptr;
}

PreparedStatement& PreparedStatement::operator=(PreparedStatement&& other) noexcept
{
    if (this != &other) {
        if (m_stmt) {
            sqlite3_finalize(m_stmt);
        }
        m_stmt = other.m_stmt;
        other.m_stmt = nullptr;
    }
    return *this;
}

void PreparedStatement::bind(int index, int value)
{
    if (m_stmt) {
        sqlite3_bind_int(m_stmt, index, value);
    }
}

void PreparedStatement::bind(int index, int64_t value)
{
    if (m_stmt) {
        sqlite3_bind_int64(m_stmt, index, value);
    }
}

void PreparedStatement::bind(int index, double value)
{
    if (m_stmt) {
        sqlite3_bind_double(m_stmt, index, value);
    }
}

void PreparedStatement::bind(int index, const std::string& value)
{
    if (m_stmt) {
        sqlite3_bind_text(m_stmt, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
    }
}

void PreparedStatement::bindNull(int index)
{
    if (m_stmt) {
        sqlite3_bind_null(m_stmt, index);
    }
}

bool PreparedStatement::step()
{
    if (!m_stmt) return false;
    int result = sqlite3_step(m_stmt);
    return result == SQLITE_ROW;
}

bool PreparedStatement::execute()
{
    if (!m_stmt) return false;
    return sqlite3_step(m_stmt) == SQLITE_DONE;
}

void PreparedStatement::reset()
{
    if (m_stmt) {
        sqlite3_reset(m_stmt);
        sqlite3_clear_bindings(m_stmt);
    }
}

int PreparedStatement::getInt(int column) const
{
    return m_stmt ? sqlite3_column_int(m_stmt, column) : 0;
}

int64_t PreparedStatement::getInt64(int column) const
{
    return m_stmt ? sqlite3_column_int64(m_stmt, column) : 0;
}

double PreparedStatement::getDouble(int column) const
{
    return m_stmt ? sqlite3_column_double(m_stmt, column) : 0.0;
}

std::string PreparedStatement::getString(int column) const
{
    if (!m_stmt) return "";
    const unsigned char* text = sqlite3_column_text(m_stmt, column);
    return text ? reinterpret_cast<const char*>(text) : "";
}

bool PreparedStatement::isNull(int column) const
{
    return m_stmt ? sqlite3_column_type(m_stmt, column) == SQLITE_NULL : true;
}

int PreparedStatement::columnCount() const
{
    return m_stmt ? sqlite3_column_count(m_stmt) : 0;
}

std::string PreparedStatement::columnName(int column) const
{
    if (!m_stmt) return "";
    const char* name = sqlite3_column_name(m_stmt, column);
    return name ? name : "";
}

// ============================================================================
// DatabaseManager Implementation
// ============================================================================

DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::~DatabaseManager()
{
    close();
}

bool DatabaseManager::open(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_db) {
        close();
    }

    int result = sqlite3_open(path.c_str(), &m_db);
    if (result != SQLITE_OK) {
        m_lastError = sqlite3_errmsg(m_db);
        LOG_ERROR("Failed to open database '%s': %s", path.c_str(), m_lastError.c_str());
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }

    // Enable foreign key support (use internal version - we already hold the lock)
    executeInternal("PRAGMA foreign_keys = ON");

    // Set busy timeout (5 seconds)
    sqlite3_busy_timeout(m_db, 5000);

    LOG_INFO("Database opened: %s", path.c_str());
    return true;
}

void DatabaseManager::close()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
        LOG_INFO("Database closed");
    }
}

QueryResult DatabaseManager::query(const std::string& sql)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_db) {
        return QueryResult(false, "Database not open");
    }

    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        m_lastError = sqlite3_errmsg(m_db);
        LOG_ERROR("Query prepare failed: %s", m_lastError.c_str());
        return QueryResult(false, m_lastError);
    }

    QueryResult queryResult(true);

    // Get column names
    int columnCount = sqlite3_column_count(stmt);
    std::vector<std::string> columns;
    columns.reserve(columnCount);
    for (int i = 0; i < columnCount; ++i) {
        const char* name = sqlite3_column_name(stmt, i);
        columns.push_back(name ? name : "");
    }

    // Fetch rows
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::vector<std::string> values;
        values.reserve(columnCount);
        for (int i = 0; i < columnCount; ++i) {
            const unsigned char* text = sqlite3_column_text(stmt, i);
            values.push_back(text ? reinterpret_cast<const char*>(text) : "");
        }
        queryResult.addRow(QueryRow(columns, values));
    }

    sqlite3_finalize(stmt);
    return queryResult;
}

bool DatabaseManager::executeInternal(const std::string& sql)
{
    // NOTE: Caller must hold m_mutex!
    if (!m_db) {
        m_lastError = "Database not open";
        return false;
    }

    char* errMsg = nullptr;
    int result = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (result != SQLITE_OK) {
        m_lastError = errMsg ? errMsg : "Unknown error";
        LOG_ERROR("Execute failed: %s", m_lastError.c_str());
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

bool DatabaseManager::execute(const std::string& sql)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return executeInternal(sql);
}

bool DatabaseManager::executeFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        m_lastError = "Failed to open SQL file: " + path;
        LOG_ERROR("%s", m_lastError.c_str());
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string sql = buffer.str();

    if (!execute(sql)) {
        LOG_ERROR("Failed to execute SQL file: %s", path.c_str());
        return false;
    }

    LOG_INFO("Executed SQL file: %s", path.c_str());
    return true;
}

PreparedStatement DatabaseManager::prepare(const std::string& sql)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_db) {
        LOG_ERROR("Cannot prepare statement: database not open");
        return PreparedStatement();
    }

    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        m_lastError = sqlite3_errmsg(m_db);
        LOG_ERROR("Prepare failed: %s", m_lastError.c_str());
        return PreparedStatement();
    }

    return PreparedStatement(stmt);
}

void DatabaseManager::beginTransaction()
{
    execute("BEGIN TRANSACTION");
}

void DatabaseManager::commit()
{
    execute("COMMIT");
}

void DatabaseManager::rollback()
{
    execute("ROLLBACK");
}

int64_t DatabaseManager::lastInsertId() const
{
    return m_db ? sqlite3_last_insert_rowid(m_db) : 0;
}

int DatabaseManager::changesCount() const
{
    return m_db ? sqlite3_changes(m_db) : 0;
}
