#pragma once

// Dreadmyst version information
namespace DmystVersion
{
    constexpr int Major = 1;
    constexpr int Minor = 0;
    constexpr int Patch = 0;
    constexpr int Build = 1;

    // Combined version number for protocol checks
    constexpr int VersionNumber = (Major * 10000) + (Minor * 100) + Patch;
}

// Legacy macro for code compatibility
#define DYMST_VERSION DmystVersion::Build
