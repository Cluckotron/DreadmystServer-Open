cmake_minimum_required(VERSION 3.16)
project(DreadmystServer VERSION 0.2.8 LANGUAGES CXX)

# C++17 standard required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type defaults to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra)
    # Note: -Wpedantic emits warnings about variadic macros with zero arguments (LOG_INFO("msg"))
    # Our Logger macros use ##__VA_ARGS__ which is a GCC/Clang extension that works correctly.
    # We accept these warnings as they don't indicate actual problems.
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-Wpedantic -Wno-gnu-zero-variadic-macro-arguments)
    else()
        # GCC: -Wpedantic warns about variadic macros but has no specific flag to silence them
        # We omit -Wpedantic to avoid noise, relying on -Wall -Wextra for quality checks
    endif()
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    else()
        add_compile_options(-O2)
    endif()
endif()

# Find required packages
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

# Shared library sources (from ../Shared)
set(SHARED_DIR "${CMAKE_SOURCE_DIR}/../Shared")
set(SHARED_SOURCES
    ${SHARED_DIR}/StlBuffer.cpp
    ${SHARED_DIR}/SfSocket.cpp
    ${SHARED_DIR}/MutualUnit.cpp
    ${SHARED_DIR}/Md5.cpp
)

# Server sources
set(SERVER_SOURCES
    src/main.cpp
    src/AI/NpcAI.cpp
    src/AI/ThreatManager.cpp
    src/Core/Config.cpp
    src/Core/GameClock.cpp
    src/Core/Logger.cpp
    src/Combat/AuraSystem.cpp
    src/Combat/CombatFormulas.cpp
    src/Combat/CombatMessenger.cpp
    src/Combat/CooldownManager.cpp
    src/Combat/SpellCaster.cpp
    src/Combat/SpellUtils.cpp
    src/Database/AccountDb.cpp
    src/Database/AsyncSaver.cpp
    src/Database/CharacterDb.cpp
    src/Database/DatabaseManager.cpp
    src/Database/GameData.cpp
    src/Handlers/AuthHandlers.cpp
    src/Handlers/CharacterHandlers.cpp
    src/Handlers/MiscHandlers.cpp
    src/Handlers/WorldHandlers.cpp
    src/Network/PacketRouter.cpp
    src/Network/PacketSender.cpp
    src/Network/Session.cpp
    src/Network/SessionManager.cpp
    src/Systems/Inventory.cpp
    src/Systems/Equipment.cpp
    src/Systems/LootSystem.cpp
    src/Systems/VendorSystem.cpp
    src/Systems/GossipSystem.cpp
    src/Systems/ExperienceSystem.cpp
    src/Systems/QuestManager.cpp
    src/Systems/PlayerQuestLog.cpp
    src/Systems/BankSystem.cpp
    src/Systems/TradeSystem.cpp
    src/Systems/ChatSystem.cpp
    src/Systems/PartySystem.cpp
    src/Systems/GuildSystem.cpp
    src/Systems/DuelSystem.cpp
    src/World/Entity.cpp
    src/World/Map.cpp
    src/World/MapManager.cpp
    src/World/Npc.cpp
    src/World/NpcSpawner.cpp
    src/World/Player.cpp
    src/World/WorldManager.cpp
)

# Create executable
add_executable(DreadmystServer
    ${SERVER_SOURCES}
    ${SHARED_SOURCES}
)

target_compile_definitions(DreadmystServer PRIVATE
    DREADMYST_HEADLESS_SERVER=1
    BOOST_ERROR_CODE_HEADER_ONLY=1
)

# Include directories
target_include_directories(DreadmystServer PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${SHARED_DIR}
)

# Link libraries
target_link_libraries(DreadmystServer PRIVATE
    SQLite::SQLite3
    Threads::Threads
)

# Precompiled header (temporarily disabled for debugging)
# if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
#     target_precompile_headers(DreadmystServer PRIVATE src/stdafx.h)
# endif()

# Copy data files to build directory
file(COPY ${CMAKE_SOURCE_DIR}/data DESTINATION ${CMAKE_BINARY_DIR})

# Installation
install(TARGETS DreadmystServer DESTINATION bin)
install(DIRECTORY data/ DESTINATION share/dreadmyst)

# Print configuration summary
message(STATUS "")
message(STATUS "=== Dreadmyst Server Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Network backend: Boost.Asio (header-only)")
message(STATUS "SQLite3 Version: ${SQLite3_VERSION}")
message(STATUS "======================================")
message(STATUS "")
