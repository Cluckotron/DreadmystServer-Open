#pragma once

#include <string>
#include <stdint.h>

// MD5 hash utility
// Used for password hashing and data verification

namespace Md5
{
    std::string hash(const std::string& input);
    std::string hashBytes(const uint8_t* data, size_t length);
}
