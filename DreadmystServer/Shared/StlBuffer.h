#pragma once

#include <vector>
#include <string>
#include <stdint.h>
#include <stdexcept>

// Binary packet serialization buffer
// Used for all network packet serialization/deserialization

class StlBuffer
{
public:
    StlBuffer() = default;
    StlBuffer(const std::vector<uint8_t>& data) : m_data(data) {}

    // Write operators
    StlBuffer& operator<<(uint8_t val);
    StlBuffer& operator<<(int8_t val);
    StlBuffer& operator<<(uint16_t val);
    StlBuffer& operator<<(int16_t val);
    StlBuffer& operator<<(uint32_t val);
    StlBuffer& operator<<(int32_t val);
    StlBuffer& operator<<(uint64_t val);
    StlBuffer& operator<<(int64_t val);
    StlBuffer& operator<<(float val);
    StlBuffer& operator<<(double val);
    StlBuffer& operator<<(bool val);
    StlBuffer& operator<<(const std::string& val);

    // Read operators
    StlBuffer& operator>>(uint8_t& val);
    StlBuffer& operator>>(int8_t& val);
    StlBuffer& operator>>(uint16_t& val);
    StlBuffer& operator>>(int16_t& val);
    StlBuffer& operator>>(uint32_t& val);
    StlBuffer& operator>>(int32_t& val);
    StlBuffer& operator>>(uint64_t& val);
    StlBuffer& operator>>(int64_t& val);
    StlBuffer& operator>>(float& val);
    StlBuffer& operator>>(double& val);
    StlBuffer& operator>>(bool& val);
    StlBuffer& operator>>(std::string& val);

    // Buffer operations
    void eraseFront(size_t bytes);
    void clear();
    size_t size() const { return m_data.size(); }
    bool empty() const { return m_data.empty(); }
    const uint8_t* data() const { return m_data.data(); }
    uint8_t* data() { return m_data.data(); }

    // For packet building - prepends size header
    StlBuffer& build(StlBuffer&& buf);

    // File I/O
    bool writeFile(const std::string& path) const;
    bool readFile(const std::string& path);

private:
    std::vector<uint8_t> m_data;
    size_t m_readPos = 0;
};
