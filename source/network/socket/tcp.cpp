#include "tcp.hpp"
#include "cassync.h"
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

torr::tcp::tcp()
{
    signal(SIGPIPE, SIG_IGN);
}

torr::tcp::~tcp()
{
    if (m_socket_fd >= 0)
        close(m_socket_fd);
}

std::expected<int, const char*>
    torr::tcp::connect(const std::string& ip_address, const size_t& port)
{
    struct in_addr connection_address;
    if (inet_aton(ip_address.c_str(), &connection_address) == 0)
        return std::unexpected("invalid ip address: inet_aton() failed");
    return m_socket_fd;
}

std::expected<int, const char*>
    torr::tcp::connect(const struct in_addr& ip_address, const size_t& port)
{
    if (m_socket_fd >= 0)
        close(m_socket_fd);

    m_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket_fd < 0)
        return std::unexpected("tcp connect: socket() failed");

    memset(&m_sockaddr_connect_to, 0, sizeof(m_sockaddr_connect_to));
    m_sockaddr_connect_to.sin_family = AF_INET;
    m_sockaddr_connect_to.sin_addr.s_addr = ip_address.s_addr;
    m_sockaddr_connect_to.sin_port = htons(port);
    ::connect_with_timeout(m_socket_fd, (struct sockaddr*)&m_sockaddr_connect_to, sizeof(m_sockaddr_connect_to), 2000);
    return m_socket_fd;
}

std::expected<size_t, const char*>
    torr::tcp::send(const uint8_t* buffer, size_t length, int flags) const
{
    int sendto_result = ::send(
        m_socket_fd,
        buffer,
        length,
        flags
    );

    if (sendto_result < 0)
        return std::unexpected("tcp send: send() failed");
    return sendto_result;
}

std::expected<size_t, const char*>
    torr::tcp::receive(uint8_t* buffer, size_t length, int flags) const
{
    socklen_t socklen = sizeof(m_sockaddr_connect_to);
    int recvfrom_result = recv(
        m_socket_fd,
        buffer,
        length,
        flags
    );

    if (recvfrom_result < 0)
        return std::unexpected("tcp receive: recv() failed");
    return recvfrom_result;
}

bool torr::tcp::set_send_timeout(size_t micro_seconds) const
{
    struct timeval timeout;      
    timeout.tv_sec = 0;
    timeout.tv_usec = micro_seconds;
    
    if (setsockopt(m_socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0)
        return false;
    return true;
}
