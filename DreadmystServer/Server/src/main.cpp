// Dreadmyst Community Server - Main Entry Point
// Headless/community-host build. The gameplay code remains the emulator's code;
// networking uses Boost.Asio so the server does not need SFML installed.

#include "stdafx.h"
#include "Core/Config.h"
#include "Core/Logger.h"
#include "Core/GameClock.h"
#include "Database/AsyncSaver.h"
#include "Database/DatabaseManager.h"
#include "Database/GameData.h"
#include "Network/Session.h"
#include "Network/SessionManager.h"
#include "Network/PacketRouter.h"
#include "World/WorldManager.h"
#include "World/MapManager.h"
#include "Systems/VendorSystem.h"
#include "Systems/GossipSystem.h"
#include "Systems/GuildSystem.h"
#include "SfSocket.h"

#include <boost/asio.hpp>
#include <atomic>
#include <csignal>

static std::atomic<bool> g_running{true};

static void signalHandler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM)
    {
        LOG_INFO("Shutdown signal received...");
        g_running = false;
    }
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    LOG_INFO("===========================================");
    LOG_INFO("  Dreadmyst Community Server v0.2.8");
    LOG_INFO("  Community Protocol v1 / authoritative host");
    LOG_INFO("===========================================");

    const char* configPath = "data/server.ini";
    if (!sConfig.load(configPath))
        LOG_WARN("Could not load %s, using defaults", configPath);

    LOG_INFO("Server Port: %d", sConfig.getServerPort());
    LOG_INFO("Max Connections: %d", sConfig.getMaxConnections());

    if (!sDatabase.open(sConfig.getServerDbPath()))
    {
        LOG_WARN("Could not open server database, creating new one");
        if (!sDatabase.open(sConfig.getServerDbPath()))
        {
            LOG_ERROR("Failed to create server database");
            return 1;
        }
    }

    if (!sDatabase.executeFile("data/schema.sql"))
        LOG_WARN("Could not execute schema.sql (may already exist)");

    if (!sGameData.loadFromDatabase(sConfig.getGameDbPath()))
    {
        LOG_ERROR("Failed to load game data from %s", sConfig.getGameDbPath().c_str());
        return 1;
    }

    sVendorManager.loadVendorData();
    sGossipManager.loadGossipData();
    sGuildManager.loadGuildsFromDatabase();
    sPacketRouter.initialize();

    const std::string mapsDir = sConfig.getMapsPath();
    if (!sMapManager.initialize(mapsDir))
    {
        LOG_WARN("MapManager initialization failed (maps may not load)");
    }
    else
    {
        sMapManager.getMap(sMapManager.getDefaultStartMapId());
    }

    sWorldManager.initialize();
    sAsyncSaver.start();
    sGameClock.setTickRate(20);
    sGameClock.start();

    using boost::asio::ip::tcp;
    boost::asio::io_context io;
    boost::system::error_code ec;
    tcp::acceptor acceptor(io);
    acceptor.open(tcp::v4(), ec);
    if (ec)
    {
        LOG_ERROR("Failed to create TCP listener: %s", ec.message().c_str());
        return 1;
    }
    acceptor.set_option(tcp::acceptor::reuse_address(true), ec);
    acceptor.bind(tcp::endpoint(tcp::v4(), sConfig.getServerPort()), ec);
    if (ec)
    {
        LOG_ERROR("Failed to bind to port %d: %s", sConfig.getServerPort(), ec.message().c_str());
        return 1;
    }
    acceptor.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec)
    {
        LOG_ERROR("Failed to listen on port %d: %s", sConfig.getServerPort(), ec.message().c_str());
        return 1;
    }
    acceptor.non_blocking(true, ec);
    if (ec)
    {
        LOG_ERROR("Failed to configure listener: %s", ec.message().c_str());
        return 1;
    }

    LOG_INFO("Listening on port %d", sConfig.getServerPort());
    LOG_INFO("Server started. Press Ctrl+C to shutdown.");

    while (g_running)
    {
        try
        {
            // Drain pending accepts without blocking the 20 Hz game loop.
            for (;;)
            {
                if (sSessionManager.getSessionCount() >= static_cast<size_t>(sConfig.getMaxConnections()))
                    break;

                auto socket = std::make_unique<SfSocket>(SfSocket::Type::ServerSide);
                boost::system::error_code acceptEc;
                if (!socket->acceptFrom(acceptor, acceptEc))
                {
                    if (acceptEc == boost::asio::error::would_block ||
                        acceptEc == boost::asio::error::try_again)
                        break;

                    LOG_WARN("Accept failed: %s", acceptEc.message().c_str());
                    break;
                }

                Session* session = sSessionManager.createSession();
                if (!session)
                {
                    socket->disconnect();
                    break;
                }

                LOG_INFO("Session %u connected from %s",
                         session->getId(), socket->getRemoteAddress().c_str());
                session->setSocket(std::move(socket));
            }

            std::vector<uint32_t> sessionsToRemove;
            sSessionManager.forEachSession([&](Session& session)
            {
                try
                {
                    SfSocket* socket = session.getSocket();
                    if (!socket || !socket->isConnected())
                    {
                        sessionsToRemove.push_back(session.getId());
                        return;
                    }

                    std::vector<std::unique_ptr<StlBuffer>> packets;
                    socket->receive(packets);

                    if (!socket->isConnected())
                    {
                        LOG_INFO("Session %u disconnected", session.getId());
                        sessionsToRemove.push_back(session.getId());
                        return;
                    }

                    for (auto& packet : packets)
                    {
                        if (packet->size() < sizeof(uint16_t))
                        {
                            LOG_WARN("Session %u: Malformed packet (size=%zu)",
                                     session.getId(), packet->size());
                            continue;
                        }

                        uint16_t opcode = 0;
                        *packet >> opcode;
                        sPacketRouter.dispatch(session, opcode, *packet);
                    }
                }
                catch (const std::exception& e)
                {
                    LOG_ERROR("Session %u: Network error: %s", session.getId(), e.what());
                    sessionsToRemove.push_back(session.getId());
                }
                catch (...)
                {
                    LOG_ERROR("Session %u: Unknown network error", session.getId());
                    sessionsToRemove.push_back(session.getId());
                }
            });

            for (uint32_t id : sessionsToRemove)
                sSessionManager.removeSession(id);

            const bool shouldTick = sGameClock.tick();
            if (shouldTick)
            {
                sSessionManager.update();

                try
                {
                    sWorldManager.update(sGameClock.getDeltaTime());
                }
                catch (const std::exception& e)
                {
                    LOG_ERROR("World update error: %s", e.what());
                }
                catch (...)
                {
                    LOG_ERROR("Unknown world update error");
                }

                static uint64_t lastStatusTick = 0;
                if (sGameClock.getTickCount() - lastStatusTick >= 60ULL * sGameClock.getTickRate())
                {
                    lastStatusTick = sGameClock.getTickCount();
                    LOG_INFO("Uptime: %s | Sessions: %zu | Ticks: %llu",
                             sGameClock.getUptimeString().c_str(),
                             sSessionManager.getSessionCount(),
                             static_cast<unsigned long long>(sGameClock.getTickCount()));
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Main loop exception: %s - server continues", e.what());
        }
        catch (...)
        {
            LOG_ERROR("Unknown main loop exception - server continues");
        }
    }

    LOG_INFO("Initiating graceful shutdown...");
    acceptor.close(ec);
    LOG_INFO("Stopped accepting connections");

    sSessionManager.disconnectAll("Server shutting down");
    sWorldManager.shutdown();
    sAsyncSaver.flush();
    sAsyncSaver.stop();
    sDatabase.close();

    LOG_INFO("Final uptime: %s", sGameClock.getUptimeString().c_str());
    LOG_INFO("Total ticks processed: %llu",
             static_cast<unsigned long long>(sGameClock.getTickCount()));
    LOG_INFO("Server stopped. Goodbye!");
    return 0;
}
