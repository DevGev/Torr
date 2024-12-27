#pragma once

#include "tcp.hpp"
#include <openssl/ssl.h>
#include <uri/url.hpp>
#include <expected>
#include <vector>
#include <span>

namespace torr {

class http {
private:
    url m_url;
    tcp m_tcp;
    std::vector<std::byte> m_data;

    size_t m_port {};
    bool m_is_connected {};
    SSL_CTX* m_ssl_ctx {};
    SSL* m_ssl_connection {};

public:
    http();
    ~http();

    void clear();
    std::expected<int, const char*> from_string(const std::string& s);
    std::expected<int, const char*> from_url(const url& u);
    std::expected<int, const char*> connect();
    std::expected<std::span<std::byte>, const char*>
        get_request();
};

};
