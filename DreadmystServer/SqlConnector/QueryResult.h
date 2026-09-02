// QueryResult.h - SQLite query wrapper for Dreadmyst client
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <sqlite3.h>

// Field value that can be accessed as different types
class FieldValue
{
public:
    FieldValue() = default;
    explicit FieldValue(const std::string& val) : m_value(val) {}

    std::string getString() const { return m_value; }
    int getInt32() const { try { return std::stoi(m_value); } catch (...) { return 0; } }
    int64_t getInt64() const { try { return std::stoll(m_value); } catch (...) { return 0; } }
    uint32_t getUInt32() const { try { return static_cast<uint32_t>(std::stoul(m_value)); } catch (...) { return 0; } }
    float getFloat() const { try { return std::stof(m_value); } catch (...) { return 0.0f; } }
    double getDouble() const { try { return std::stod(m_value); } catch (...) { return 0.0; } }
    bool getBool() const { return getInt32() != 0; }

private:
    std::string m_value;
};

// Row of field values with indexed access
class RowData
{
public:
    RowData() = default;

    void addField(const std::string& value) { m_fields.emplace_back(value); }

    const FieldValue& operator[](size_t index) const
    {
        static FieldValue empty;
        return index < m_fields.size() ? m_fields[index] : empty;
    }

    size_t size() const { return m_fields.size(); }

private:
    std::vector<FieldValue> m_fields;
};

class QueryResult
{
public:
    QueryResult() = default;
    ~QueryResult()
    {
        if (m_stmt)
            sqlite3_finalize(m_stmt);
    }

    // Move only
    QueryResult(const QueryResult&) = delete;
    QueryResult& operator=(const QueryResult&) = delete;
    QueryResult(QueryResult&& other) noexcept
        : m_stmt(other.m_stmt), m_hasRow(other.m_hasRow)
    {
        other.m_stmt = nullptr;
    }
    QueryResult& operator=(QueryResult&& other) noexcept
    {
        if (this != &other)
        {
            if (m_stmt) sqlite3_finalize(m_stmt);
            m_stmt = other.m_stmt;
            m_hasRow = other.m_hasRow;
            other.m_stmt = nullptr;
        }
        return *this;
    }

    bool valid() const { return m_stmt != nullptr; }
    bool hasRow() const { return m_hasRow; }

    bool next()
    {
        if (!m_stmt) return false;
        int result = sqlite3_step(m_stmt);
        m_hasRow = (result == SQLITE_ROW);
        return m_hasRow;
    }

    int getInt(int column) const
    {
        return m_stmt ? sqlite3_column_int(m_stmt, column) : 0;
    }

    int64_t getInt64(int column) const
    {
        return m_stmt ? sqlite3_column_int64(m_stmt, column) : 0;
    }

    double getDouble(int column) const
    {
        return m_stmt ? sqlite3_column_double(m_stmt, column) : 0.0;
    }

    float getFloat(int column) const
    {
        return static_cast<float>(getDouble(column));
    }

    std::string getString(int column) const
    {
        if (!m_stmt) return "";
        const unsigned char* text = sqlite3_column_text(m_stmt, column);
        return text ? reinterpret_cast<const char*>(text) : "";
    }

    int columnCount() const
    {
        return m_stmt ? sqlite3_column_count(m_stmt) : 0;
    }

    std::string columnName(int column) const
    {
        if (!m_stmt) return "";
        const char* name = sqlite3_column_name(m_stmt, column);
        return name ? name : "";
    }

    // For internal use by database connection
    void setStatement(sqlite3_stmt* stmt)
    {
        if (m_stmt) sqlite3_finalize(m_stmt);
        m_stmt = stmt;
        m_hasRow = false;
    }

    // Indexed data access - caches all results for keyed lookup
    // Usage: result.data(spellId, "column_name") -> returns string value
    void buildIndex(int keyColumn = 0)
    {
        if (!m_stmt || m_indexed) return;

        // Get column names
        int colCount = columnCount();
        std::vector<std::string> colNames;
        for (int i = 0; i < colCount; ++i)
            colNames.push_back(columnName(i));

        // Read all rows
        while (next())
        {
            // Build row data for fetchData() access
            RowData row;
            for (int i = 0; i < colCount; ++i)
                row.addField(getString(i));
            m_rowData.push_back(std::move(row));

            // Build indexed data for data() access
            int key = getInt(keyColumn);
            for (int i = 0; i < colCount; ++i)
            {
                m_indexedData[key][colNames[i]] = getString(i);
            }
        }
        m_indexed = true;
    }

    std::string data(int key, const std::string& column) const
    {
        auto rowIt = m_indexedData.find(key);
        if (rowIt == m_indexedData.end()) return "";
        auto colIt = rowIt->second.find(column);
        if (colIt == rowIt->second.end()) return "";
        return colIt->second;
    }

    bool hasKey(int key) const
    {
        return m_indexedData.find(key) != m_indexedData.end();
    }

    // Row-based data access for iteration
    // Usage: for (auto& row : result.fetchData()) { row[0].getString(); }
    const std::vector<RowData>& fetchData() const
    {
        return m_rowData;
    }

private:
    sqlite3_stmt* m_stmt = nullptr;
    bool m_hasRow = false;
    bool m_indexed = false;
    std::map<int, std::map<std::string, std::string>> m_indexedData;
    std::vector<RowData> m_rowData;
};

// Simple SQLite database connection
class SqliteConnection
{
public:
    SqliteConnection() = default;
    ~SqliteConnection()
    {
        close();
    }

    bool open(const std::string& path)
    {
        close();
        int result = sqlite3_open(path.c_str(), &m_db);
        return result == SQLITE_OK;
    }

    void close()
    {
        if (m_db)
        {
            sqlite3_close(m_db);
            m_db = nullptr;
        }
    }

    bool isOpen() const { return m_db != nullptr; }

    // Get raw database handle (for SqlConnector factory pattern)
    sqlite3* getDb() const { return m_db; }

    QueryResult query(const std::string& sql)
    {
        QueryResult result;
        if (!m_db) return result;

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK)
        {
            result.setStatement(stmt);
        }
        return result;
    }

    bool execute(const std::string& sql)
    {
        if (!m_db) return false;
        char* errMsg = nullptr;
        int result = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &errMsg);
        if (errMsg) sqlite3_free(errMsg);
        return result == SQLITE_OK;
    }

    int64_t lastInsertId() const
    {
        return m_db ? sqlite3_last_insert_rowid(m_db) : 0;
    }

    std::string lastError() const
    {
        return m_db ? sqlite3_errmsg(m_db) : "No database connection";
    }

private:
    sqlite3* m_db = nullptr;
};
