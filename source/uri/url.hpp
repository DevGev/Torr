#pragma once

#include <memory>
#include <string>
#include <expected>

/* URL_REGEX_PATTERN:
 * group 2: protocol
 * group 3: host
 * group 6: path
 * group 7: last subdirectory
 * group 8: query string
 * https://regex101.com/r/SAOoIt/1 */
#define URL_REGEX_PATTERN \
    R"(^((http[s]?|ftp|udp):\/)?\/?([^:\/\s]+)(:([^\/]*))?((\/\w+)*\/)?([\w\-\.]+[^#?\s]+)?(.*))"

namespace torr {

class url {
public:
    enum class protocol_type {
        https, http, udp, tcp, wss,
    };

private:
    protocol_type m_protocol;
    std::string m_host;
    std::string m_path;
    std::string m_query;
    size_t m_port;

public:
    url();
    ~url();

    const protocol_type protocol() const;
    const std::string& host() const;
    const size_t port() const;
    const std::string& path() const;
    const std::string& query() const;
    const std::string decode(const std::string&) const;

    const std::expected<protocol_type, const char*>
        determine_protocol(const std::string& protocol_as_string) const;
    const std::expected<size_t, const char*>
        determine_port(const std::string& port_as_string, protocol_type protocol) const;
    const std::expected<url, const char*>
        from_string(const std::string&);

    static std::string bytes_as_url_escaped_string(std::span<const std::byte> bytes);
};

}
