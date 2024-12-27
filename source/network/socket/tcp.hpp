#pragma once

#include <expected>
#include <string>
#include <netinet/in.h>

namespace torr {

class tcp {
private:
    struct sockaddr_in m_sockaddr_connect_to {};
    int m_socket_fd { -1 };

public:
    tcp();
    ~tcp();

    std::expected<int, const char*>
        connect(const std::string& ip_address, const size_t& port);
    std::expected<int, const char*>
        connect(const struct in_addr& ip_address, const size_t& port);

    std::expected<size_t, const char*>
        send(const uint8_t* buffer, size_t length, int flags = 0) const;
    std::expected<size_t, const char*>
        receive(uint8_t* buffer, size_t length, int flags = 0) const;

    bool set_send_timeout(size_t micro_seconds) const;
    int socket_file_descriptor() const { return m_socket_fd; }
};

}
