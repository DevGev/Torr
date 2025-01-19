#include "url.hpp"
#include <regex>
#include <string.h>
#include <unordered_map>

torr::url::url() {}
torr::url::~url() {}

const std::expected<torr::url::protocol_type, const char*>
    torr::url::determine_protocol(const std::string& protocol_as_string) const
{
    static const std::unordered_map<std::string, protocol_type> protocol_string_map = {
        { "https", protocol_type::https },
        { "http", protocol_type::http },
        { "tcp", protocol_type::tcp },
        { "udp", protocol_type::udp },
        { "wss", protocol_type::wss },
    };

    if (!protocol_string_map.contains(protocol_as_string))
        return std::unexpected("invalid url: protocol is not supported");
    return protocol_string_map.at(protocol_as_string);
}

const std::expected<size_t, const char*>
    torr::url::determine_port(const std::string& port_as_string, protocol_type protocol) const
{
    if (strspn(port_as_string.c_str(), "0123456789") != port_as_string.size()
        || port_as_string.empty()) {

        /* default ports fallback */
        if (protocol == protocol_type::https) return 443;
        if (protocol == protocol_type::http) return 80;
        else return std::unexpected("invalid url: could not determine port");
    }

    return std::stoi(port_as_string);
}

const std::expected<torr::url, const char*> 
    torr::url::from_string(const std::string& url_string)
{
    std::string regex_string = decode(url_string);
    std::smatch regex_matches;
    const std::regex regex_pattern(URL_REGEX_PATTERN);

    if (std::regex_search(regex_string, regex_matches, regex_pattern)) {

        /* should not be reachable, is URL_REGEX_PATTERN correct? */
        if (regex_matches.size() <= 8)
            return std::unexpected("invalid url: malformed url");

        /* host: required */
        m_host = regex_matches[3];
        if (m_host.empty())
            return std::unexpected("invalid url: could not determine host");

        /* protocol: optional, default http */
        m_protocol = determine_protocol(regex_matches[2].str()).value_or(protocol_type::http);

        /* port: required if not http(s) */
        auto port_or_error = determine_port(regex_matches[5].str(), m_protocol);
        if (!port_or_error.has_value())
            return std::unexpected(port_or_error.error());
        m_port = port_or_error.value();

        /* path: optional */
        m_path = regex_matches[6];
        m_path += regex_matches[8];

        /* query: optional */
        m_query = regex_matches[9];

    } else {

        /* should be reachable */
        return std::unexpected("invalid url: URL_REGEX_PATTERN regex failed");

    }

    return *this;
}

const std::string torr::url::decode(const std::string& url_string) const
{
    std::string decoded;
    for (int i = 0; i < url_string.length(); i++) {
        if (url_string[i] == '%') {
            int as_hex;
            sscanf(url_string.substr(i + 1, 2).c_str(), "%x", &as_hex);
            if (as_hex != 0x3A && as_hex != 0x2F) {
                decoded += url_string[i];
                continue;
            }
            decoded += static_cast<char>(as_hex);
            i += 2;
            continue;
        }
        decoded += url_string[i];
    }
    return decoded;
}

std::string torr::url::bytes_as_url_escaped_string(std::span<const std::byte> bytes)
{
    std::string escaped_string;
    escaped_string.reserve(bytes.size() * 3);
    for (const auto& e : bytes) {
        char hex_string[3];
        sprintf(hex_string, "%02x", (int)e);
        escaped_string += "%";
        escaped_string += hex_string;
    }
    return escaped_string;
}


/* functions below are getters */

const torr::url::protocol_type torr::url::protocol() const
{
    return m_protocol;
}

const std::string& torr::url::host() const
{
    return m_host;
}

const size_t torr::url::port() const
{
    return m_port;
}

const std::string& torr::url::path() const
{
    return m_path;
}

const std::string& torr::url::query() const
{
    return m_query;
}
