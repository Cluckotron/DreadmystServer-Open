#pragma once

#include "StlBuffer.h"
#include <memory>
#include <string>
#include <vector>

#ifdef DREADMYST_HEADLESS_SERVER

#ifndef BOOST_ERROR_CODE_HEADER_ONLY
#define BOOST_ERROR_CODE_HEADER_ONLY
#endif
#include <boost/asio.hpp>

// Server-side socket wrapper used by the community server build.  It deliberately
// keeps the legacy SfSocket name so the rest of the emulator does not need to
// know which networking backend is in use.
class SfSocket
{
public:
    enum class Type
    {
        ClientSide,
        ServerSide,
        SocketType_ClientSide = ClientSide,
        SocketType_ServerSide = ServerSide
    };

    explicit SfSocket(Type type);
    ~SfSocket();

    bool send(const StlBuffer& data);
    void sendPacket(StlBuffer data) { send(data); }

    void receive(std::vector<std::unique_ptr<StlBuffer>>& output);
    void popReceived(std::vector<std::unique_ptr<StlBuffer>>& output) { receive(output); }

    bool isConnected() const;
    bool connected() const { return isConnected(); }
    void disconnect();
    void cancel() { disconnect(); }
    std::string getRemoteAddress() const;
    bool update();

    // Server accept helper.  The acceptor must already be non-blocking.
    bool acceptFrom(boost::asio::ip::tcp::acceptor& acceptor, boost::system::error_code& ec);

private:
    static boost::asio::io_context& ioContext();

    Type m_type;
    boost::asio::ip::tcp::socket m_socket;
    bool m_connected = false;
    StlBuffer m_recvBuffer;
};

#else

#include <SFML/Network.hpp>

// Extended socket type expected by the released r1189 client source.
class TcpSocketEx : public sf::TcpSocket
{
public:
    TcpSocketEx() = default;
    ~TcpSocketEx() = default;
};

class SfSocket
{
public:
    enum class Type
    {
        ClientSide,
        ServerSide,
        SocketType_ClientSide = ClientSide,
        SocketType_ServerSide = ServerSide
    };

    explicit SfSocket(Type type);
    SfSocket(std::shared_ptr<sf::TcpSocket> socket, Type type);
    ~SfSocket();

    bool send(const StlBuffer& data);
    void sendPacket(StlBuffer data) { send(data); }

    void receive(std::vector<std::unique_ptr<StlBuffer>>& output);
    void popReceived(std::vector<std::unique_ptr<StlBuffer>>& output) { receive(output); }

    bool isConnected() const;
    bool connected() const { return isConnected(); }
    void disconnect();
    void cancel() { disconnect(); }
    std::string getRemoteAddress() const;
    bool update();

    sf::TcpSocket* getSocket();

private:
    Type m_type;
    std::unique_ptr<sf::TcpSocket> m_ownedSocket;
    std::shared_ptr<sf::TcpSocket> m_sharedSocket;
    sf::TcpSocket* m_socket = nullptr;
    StlBuffer m_recvBuffer;
};

#endif
