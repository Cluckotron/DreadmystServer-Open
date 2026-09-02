// StlBuffer - Binary packet serialization
// TODO: Implement fully in Tasks 1.2-1.4

#include "StlBuffer.h"
#include <cstring>
#include <cstdio>

// Write operators - TODO: Task 1.2
StlBuffer& StlBuffer::operator<<(uint8_t val)
{
    m_data.push_back(val);
    return *this;
}

StlBuffer& StlBuffer::operator<<(int8_t val)
{
    return *this << static_cast<uint8_t>(val);
}

StlBuffer& StlBuffer::operator<<(uint16_t val)
{
    m_data.push_back(static_cast<uint8_t>(val & 0xFF));
    m_data.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    return *this;
}

StlBuffer& StlBuffer::operator<<(int16_t val)
{
    return *this << static_cast<uint16_t>(val);
}

StlBuffer& StlBuffer::operator<<(uint32_t val)
{
    m_data.push_back(static_cast<uint8_t>(val & 0xFF));
    m_data.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    m_data.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    m_data.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    return *this;
}

StlBuffer& StlBuffer::operator<<(int32_t val)
{
    return *this << static_cast<uint32_t>(val);
}

StlBuffer& StlBuffer::operator<<(uint64_t val)
{
    *this << static_cast<uint32_t>(val & 0xFFFFFFFF);
    *this << static_cast<uint32_t>((val >> 32) & 0xFFFFFFFF);
    return *this;
}

StlBuffer& StlBuffer::operator<<(int64_t val)
{
    return *this << static_cast<uint64_t>(val);
}

StlBuffer& StlBuffer::operator<<(float val)
{
    uint32_t bits;
    std::memcpy(&bits, &val, sizeof(bits));
    return *this << bits;
}

StlBuffer& StlBuffer::operator<<(double val)
{
    uint64_t bits;
    std::memcpy(&bits, &val, sizeof(bits));
    return *this << bits;
}

StlBuffer& StlBuffer::operator<<(bool val)
{
    return *this << static_cast<uint8_t>(val ? 1 : 0);
}

StlBuffer& StlBuffer::operator<<(const std::string& val)
{
    *this << static_cast<uint16_t>(val.size());
    m_data.insert(m_data.end(), val.begin(), val.end());
    return *this;
}

// Read operators - TODO: Task 1.3
StlBuffer& StlBuffer::operator>>(uint8_t& val)
{
    if (m_readPos < m_data.size()) {
        val = m_data[m_readPos++];
    } else {
        val = 0;
    }
    return *this;
}

StlBuffer& StlBuffer::operator>>(int8_t& val)
{
    uint8_t u;
    *this >> u;
    val = static_cast<int8_t>(u);
    return *this;
}

StlBuffer& StlBuffer::operator>>(uint16_t& val)
{
    uint8_t b0, b1;
    *this >> b0 >> b1;
    val = static_cast<uint16_t>(b0) | (static_cast<uint16_t>(b1) << 8);
    return *this;
}

StlBuffer& StlBuffer::operator>>(int16_t& val)
{
    uint16_t u;
    *this >> u;
    val = static_cast<int16_t>(u);
    return *this;
}

StlBuffer& StlBuffer::operator>>(uint32_t& val)
{
    uint8_t b0, b1, b2, b3;
    *this >> b0 >> b1 >> b2 >> b3;
    val = static_cast<uint32_t>(b0) |
          (static_cast<uint32_t>(b1) << 8) |
          (static_cast<uint32_t>(b2) << 16) |
          (static_cast<uint32_t>(b3) << 24);
    return *this;
}

StlBuffer& StlBuffer::operator>>(int32_t& val)
{
    uint32_t u;
    *this >> u;
    val = static_cast<int32_t>(u);
    return *this;
}

StlBuffer& StlBuffer::operator>>(uint64_t& val)
{
    uint32_t lo, hi;
    *this >> lo >> hi;
    val = static_cast<uint64_t>(lo) | (static_cast<uint64_t>(hi) << 32);
    return *this;
}

StlBuffer& StlBuffer::operator>>(int64_t& val)
{
    uint64_t u;
    *this >> u;
    val = static_cast<int64_t>(u);
    return *this;
}

StlBuffer& StlBuffer::operator>>(float& val)
{
    uint32_t bits;
    *this >> bits;
    std::memcpy(&val, &bits, sizeof(val));
    return *this;
}

StlBuffer& StlBuffer::operator>>(double& val)
{
    uint64_t bits;
    *this >> bits;
    std::memcpy(&val, &bits, sizeof(val));
    return *this;
}

StlBuffer& StlBuffer::operator>>(bool& val)
{
    uint8_t b;
    *this >> b;
    val = (b != 0);
    return *this;
}

StlBuffer& StlBuffer::operator>>(std::string& val)
{
    uint16_t len;
    *this >> len;
    if (m_readPos + len <= m_data.size()) {
        val.assign(reinterpret_cast<const char*>(&m_data[m_readPos]), len);
        m_readPos += len;
    } else {
        val.clear();
    }
    return *this;
}

// Utility methods - TODO: Task 1.4
void StlBuffer::eraseFront(size_t bytes)
{
    if (bytes >= m_data.size()) {
        m_data.clear();
        m_readPos = 0;
    } else {
        m_data.erase(m_data.begin(), m_data.begin() + bytes);
        m_readPos = (m_readPos > bytes) ? m_readPos - bytes : 0;
    }
}

void StlBuffer::clear()
{
    m_data.clear();
    m_readPos = 0;
}

StlBuffer& StlBuffer::build(StlBuffer&& buf)
{
    // Prepend size header (total packet size including header)
    uint16_t packetSize = static_cast<uint16_t>(buf.size() + sizeof(uint16_t));
    *this << packetSize;
    m_data.insert(m_data.end(), buf.m_data.begin(), buf.m_data.end());
    return *this;
}

// File I/O
bool StlBuffer::writeFile(const std::string& path) const
{
    FILE* file = fopen(path.c_str(), "wb");
    if (!file)
        return false;
    bool ok = fwrite(m_data.data(), 1, m_data.size(), file) == m_data.size();
    fclose(file);
    return ok;
}

bool StlBuffer::readFile(const std::string& path)
{
    FILE* file = fopen(path.c_str(), "rb");
    if (!file)
        return false;
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    m_data.resize(static_cast<size_t>(fileSize));
    bool ok = fread(m_data.data(), 1, fileSize, file) == static_cast<size_t>(fileSize);
    fclose(file);
    m_readPos = 0;
    return ok;
}
