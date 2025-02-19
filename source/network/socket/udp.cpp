#include "udp.hpp"
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

torr::udp::udp() {}

torr::udp::~udp()
{
    if (m_socket_fd >= 0)
        close(m_socket_fd);
}

std::expected<torr::udp*, const char*>
    torr::udp::connect(const std::string& ip_address, const size_t& port)
{
    struct in_addr connection_address;
    if (inet_aton(ip_address.c_str(), &connection_address) == 0)
        return std::unexpected("invalid ip address: inet_aton() failed");
    return connect(connection_address, port);
}

std::expected<torr::udp*, const char*>
    torr::udp::connect(const struct in_addr& ip_address, const size_t& port)
{
    m_socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket_fd < 0)
        return std::unexpected("udp connect: socket() failed");

    memset(&m_sockaddr_connect_to, 0, sizeof(m_sockaddr_connect_to));
    m_sockaddr_connect_to.sin_family = AF_INET;
    m_sockaddr_connect_to.sin_addr.s_addr = ip_address.s_addr;
    m_sockaddr_connect_to.sin_port = htons(port);
    return this;
}

std::expected<size_t, const char*>
    torr::udp::send(const uint8_t* buffer, size_t length, int flags) const
{
    int sendto_result = sendto(
        m_socket_fd,
        buffer,
        length,
        flags,
        (struct sockaddr*)&m_sockaddr_connect_to,
        sizeof(m_sockaddr_connect_to)
    );

    if (sendto_result < 0)
        return std::unexpected("udp send: sendto() failed");
    return sendto_result;
}

std::expected<size_t, const char*>
    torr::udp::receive(uint8_t* buffer, size_t length, int flags, uint32_t timeout_seconds) const
{
    struct timeval tv;
    tv.tv_sec = timeout_seconds;

    if (timeout_seconds && setsockopt(m_socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        return std::unexpected("udp receive: setsockopt() failed");

    socklen_t socklen = sizeof(m_sockaddr_connect_to);
    int recvfrom_result = recvfrom(
        m_socket_fd,
        buffer,
        length,
        flags,
        (struct sockaddr*)&m_sockaddr_connect_to,
        &socklen
    );

    tv.tv_sec = 0;
    if (timeout_seconds && setsockopt(m_socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        return std::unexpected("udp receive: setsockopt() failed");

    if (recvfrom_result < 0)
        return std::unexpected("udp receive: recvfrom() failed");
    return recvfrom_result;
}
