#include "SfSocket.h"

#ifdef DREADMYST_HEADLESS_SERVER

#include <chrono>
#include <thread>

using boost::asio::ip::tcp;

boost::asio::io_context& SfSocket::ioContext()
{
    static boost::asio::io_context io;
    return io;
}

SfSocket::SfSocket(Type type)
    : m_type(type), m_socket(ioContext())
{
}

SfSocket::~SfSocket()
{
    disconnect();
}

bool SfSocket::acceptFrom(tcp::acceptor& acceptor, boost::system::error_code& ec)
{
    acceptor.accept(m_socket, ec);
    if (ec)
        return false;

    m_socket.non_blocking(true, ec);
    if (ec)
    {
        disconnect();
        return false;
    }

    m_connected = true;
    return true;
}

bool SfSocket::send(const StlBuffer& data)
{
    if (!isConnected())
        return false;

    // Private Protocol v1 framing:
    //   uint16 little-endian total frame size (header + payload)
    //   payload begins with uint16 little-endian opcode
    if (data.size() + 2 > 0xFFFFu)
        return false;

    std::vector<uint8_t> frame;
    frame.reserve(data.size() + 2);
    const uint16_t frameSize = static_cast<uint16_t>(data.size() + 2);
    frame.push_back(static_cast<uint8_t>(frameSize & 0xFF));
    frame.push_back(static_cast<uint8_t>((frameSize >> 8) & 0xFF));
    frame.insert(frame.end(), data.data(), data.data() + data.size());

    boost::system::error_code ec;
    // Temporarily use blocking mode so a logical packet is written atomically
    // from the emulator's point of view. The server is single-threaded and
    // packets are small, so this is preferable to silently dropping Partial.
    m_socket.non_blocking(false, ec);
    if (ec)
    {
        disconnect();
        return false;
    }

    boost::asio::write(m_socket, boost::asio::buffer(frame), ec);

    boost::system::error_code restoreEc;
    m_socket.non_blocking(true, restoreEc);

    if (ec || restoreEc)
    {
        disconnect();
        return false;
    }

    return true;
}

void SfSocket::receive(std::vector<std::unique_ptr<StlBuffer>>& output)
{
    if (!isConnected())
        return;

    uint8_t temp[8192];
    for (;;)
    {
        boost::system::error_code ec;
        const std::size_t n = m_socket.read_some(boost::asio::buffer(temp), ec);

        if (!ec && n > 0)
        {
            for (std::size_t i = 0; i < n; ++i)
                m_recvBuffer << temp[i];
            continue;
        }

        if (ec == boost::asio::error::would_block || ec == boost::asio::error::try_again)
            break;

        if (ec == boost::asio::error::eof ||
            ec == boost::asio::error::connection_reset ||
            ec == boost::asio::error::connection_aborted ||
            ec == boost::asio::error::broken_pipe)
        {
            disconnect();
            return;
        }

        if (ec)
        {
            disconnect();
            return;
        }

        break;
    }

    while (m_recvBuffer.size() >= 4)
    {
        const uint16_t packetSize = static_cast<uint16_t>(m_recvBuffer.data()[0]) |
                                    (static_cast<uint16_t>(m_recvBuffer.data()[1]) << 8);

        if (packetSize < 4 || packetSize > 65000)
        {
            disconnect();
            return;
        }

        if (packetSize > m_recvBuffer.size())
            break;

        std::vector<uint8_t> payload(m_recvBuffer.data() + 2,
                                     m_recvBuffer.data() + packetSize);
        output.emplace_back(std::make_unique<StlBuffer>(payload));
        m_recvBuffer.eraseFront(packetSize);
    }
}

bool SfSocket::isConnected() const
{
    return m_connected && m_socket.is_open();
}

void SfSocket::disconnect()
{
    if (!m_socket.is_open())
    {
        m_connected = false;
        return;
    }

    boost::system::error_code ec;
    m_socket.shutdown(tcp::socket::shutdown_both, ec);
    ec.clear();
    m_socket.close(ec);
    m_connected = false;
}

std::string SfSocket::getRemoteAddress() const
{
    if (!isConnected())
        return {};

    boost::system::error_code ec;
    auto ep = m_socket.remote_endpoint(ec);
    if (ec)
        return {};
    return ep.address().to_string();
}

bool SfSocket::update()
{
    return isConnected();
}

#else

SfSocket::SfSocket(Type type)
    : m_type(type), m_ownedSocket(std::make_unique<sf::TcpSocket>())
{
    m_socket = m_ownedSocket.get();
    m_socket->setBlocking(false);
}

SfSocket::SfSocket(std::shared_ptr<sf::TcpSocket> socket, Type type)
    : m_type(type), m_sharedSocket(std::move(socket))
{
    m_socket = m_sharedSocket.get();
}

SfSocket::~SfSocket()
{
    disconnect();
}

bool SfSocket::update()
{
    return isConnected();
}

bool SfSocket::send(const StlBuffer& data)
{
    if (!m_socket || !isConnected())
        return false;

    StlBuffer packet;
    packet.build(StlBuffer(std::vector<uint8_t>(data.data(), data.data() + data.size())));

    size_t total = 0;
    while (total < packet.size())
    {
        size_t sent = 0;
        const auto status = m_socket->send(packet.data() + total, packet.size() - total, sent);
        total += sent;
        if (status == sf::Socket::Done)
            break;
        if (status == sf::Socket::Partial)
            continue;
        if (status == sf::Socket::NotReady)
            return false;
        disconnect();
        return false;
    }
    return total == packet.size();
}

void SfSocket::receive(std::vector<std::unique_ptr<StlBuffer>>& output)
{
    if (!m_socket)
        return;

    for (;;)
    {
        uint8_t tempBuf[8192];
        size_t received = 0;
        const auto status = m_socket->receive(tempBuf, sizeof(tempBuf), received);

        if (status == sf::Socket::Done && received > 0)
        {
            for (size_t i = 0; i < received; ++i)
                m_recvBuffer << tempBuf[i];
            continue;
        }

        if (status == sf::Socket::Disconnected || status == sf::Socket::Error)
        {
            disconnect();
            return;
        }
        break;
    }

    while (m_recvBuffer.size() >= 4)
    {
        const uint16_t packetSize = static_cast<uint16_t>(m_recvBuffer.data()[0]) |
                                    (static_cast<uint16_t>(m_recvBuffer.data()[1]) << 8);
        if (packetSize < 4 || packetSize > 65000)
        {
            disconnect();
            return;
        }
        if (packetSize > m_recvBuffer.size())
            break;

        auto pkt = std::make_unique<StlBuffer>(
            std::vector<uint8_t>(m_recvBuffer.data() + 2, m_recvBuffer.data() + packetSize));
        output.emplace_back(std::move(pkt));
        m_recvBuffer.eraseFront(packetSize);
    }
}

bool SfSocket::isConnected() const
{
    return m_socket && m_socket->getRemoteAddress() != sf::IpAddress::None;
}

void SfSocket::disconnect()
{
    if (m_socket)
        m_socket->disconnect();
}

std::string SfSocket::getRemoteAddress() const
{
    if (!m_socket)
        return {};
    return m_socket->getRemoteAddress().toString();
}

sf::TcpSocket* SfSocket::getSocket()
{
    return m_socket;
}

#endif
