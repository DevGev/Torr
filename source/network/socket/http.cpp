#include "http.hpp"
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <format>
#include <fstream>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <unistd.h>

torr::http::http()
{

}

torr::http::~http()
{
    clear();
}

std::expected<int, const char*> torr::http::from_string(const std::string& s)
{
    m_url.from_string(s);
    return connect();
}

std::expected<int, const char*> torr::http::from_url(const url& u)
{
    m_url = u;
    return connect();
}

std::expected<int, const char*> torr::http::connect()
{
    if (m_is_connected)
        clear();

    SSL_load_error_strings();
    SSL_library_init();
    m_ssl_ctx = SSL_CTX_new(SSLv23_client_method());
    m_port = m_url.port();

    if (m_url.host().empty())
        return std::unexpected("http from url: invalid url");

    struct hostent* hostent = gethostbyname(m_url.host().c_str());
    if (hostent == NULL)
        return std::unexpected("http from url: gethostbyname() failed");
    struct in_addr in_addr;
    in_addr.s_addr = ((struct in_addr*)hostent->h_addr)->s_addr;

    if (!m_tcp.connect(in_addr, m_port))
        return std::unexpected("http from url: could not connect() tcp socket");

    m_ssl_connection = SSL_new(m_ssl_ctx);
    SSL_set_fd(m_ssl_connection, m_tcp.socket_file_descriptor());
    if (m_port == 443) {
        int err = SSL_connect(m_ssl_connection);
        if (err != 1) return std::unexpected("http from url: openssl connect() failed");
    }

    m_is_connected = true;
    return m_tcp.socket_file_descriptor();
}

std::expected<std::span<std::byte>, const char*>
    torr::http::get_request()
{
    if (!m_is_connected)
        return std::unexpected("http request: not connected to any socket");

    auto tcp_write = [&](uint8_t* buffer, size_t size) {
        if (m_port == 443) {
            int error = SSL_write(m_ssl_connection, buffer, size);
            error = (error < 0) ? 0 : error;
            return (size_t)error;
        }
        return m_tcp.send(buffer, size).value();
    };

    auto tcp_receive = [&](uint8_t* buffer, size_t size) {
        if (m_port == 443) {
            int error = SSL_read(m_ssl_connection, buffer, size);
            error = (error < 0) ? 0 : error;
            return (size_t)error;
        }
        return m_tcp.receive(buffer, size).value();
    };

    std::string request = std::format(
        "GET {}{} HTTP/1.1\r\nHost: {}\r\nConnection: close\r\n\r\n",
        m_url.path(),
        m_url.query(),
        m_url.host()
    );

    auto send_size = tcp_write((uint8_t*)request.data(), request.size());
    if (!send_size)
        return std::unexpected("http get request: could not send() to tcp socket");

    m_data.resize(1024);
    size_t read_offset = 0;
    while (int size = tcp_receive((uint8_t*)m_data.data() + read_offset, 1024)) {
        if (size <= 0) break;
        read_offset += size;
        m_data.resize(m_data.size() + size);
    }

    uint32_t header_end = 4;
    for (size_t i = 0; i < m_data.size(); ++i) {
        if ((char)m_data.at(i) == '\r' ||
            (char)m_data.at(i) == '\n')
            header_end--;
        else
            header_end = 4;

        if (!header_end) {
            m_data.erase(m_data.begin(), m_data.begin() + i);
            if (!m_data.empty()) m_data.erase(m_data.begin());
            break;
        }
    }

    return m_data;
}

void torr::http::clear()
{
    m_tcp = {}; 
    m_data.clear();

    ERR_free_strings();
    EVP_cleanup();

    if (m_ssl_ctx)
        SSL_CTX_free(m_ssl_ctx);
    if (m_ssl_connection) {
        SSL_certs_clear(m_ssl_connection);
        SSL_clear(m_ssl_connection);
        SSL_free(m_ssl_connection);
    }

    CRYPTO_cleanup_all_ex_data();
}
