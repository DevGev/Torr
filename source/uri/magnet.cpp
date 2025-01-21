#include "magnet.hpp"
#include <generic/try.hpp>
#include <network/socket/http.hpp>
#include <ranges>
#include <print>
#include <regex>

torr::magnet::magnet() {}
torr::magnet::~magnet() {}

std::unique_ptr<torr::torrent_source>
    torr::magnet::copy() const
{
    return std::move(
        std::unique_ptr<torr::torrent_source>
        (new magnet(*this))
    );
}

std::optional<const std::vector<std::byte>*>
    torr::magnet::file_hash() const
{
    return &m_file_hash;
}

std::optional<const std::string*>
    torr::magnet::file_name() const
{
    return &m_file_name;
}

std::optional<size_t>
    torr::magnet::piece_length() const
{
    return m_piece_length;
}

const std::vector<torr::tracker>&
    torr::magnet::trackers() const
{
    return m_trackers;
}

torr::torrent_source::source_type torr::magnet::type() const
{
    return torr::torrent_source::source_type::magnet_uri;
}

const std::expected<torr::magnet*, const char*>
    torr::magnet::urn_to_file_hash(const std::string& urn_string)
{
    std::string hash_as_hex;
    std::string regex_string = ":" + urn_string + ":";
    std::smatch regex_matches;
    const std::regex regex_pattern(R"((?:)[a-zA-Z0-9]+(?:))");

    enum class resource_type { unknown, urn };
    enum class protocol_type { unknown, bit_torrent };

    resource_type resource = resource_type::unknown;
    protocol_type protocol = protocol_type::unknown;

    while (std::regex_search(regex_string, regex_matches, regex_pattern)) {
        if (regex_matches.size() <= 0) {
            regex_string = regex_matches.suffix().str();
            continue;
        }

        else if (regex_matches[0].str() == "urn")
            resource = resource_type::urn;
        else if (regex_matches[0].str() == "btih")
            protocol = protocol_type::bit_torrent;
        else
            hash_as_hex = regex_matches[0].str();

        regex_string = regex_matches.suffix().str();
    }

    auto hex_string_to_bytes = hash_as_hex
        | std::views::transform([](char c) {
            return c >= '0' && c <= '9' ? c - '0' : 10 + c - 'a';
        })
        | std::views::adjacent_transform<2>([](unsigned char hi, unsigned char lo) {
            return std::byte(hi << 4 | lo);
        });

    for (size_t i = 0; i < hash_as_hex.size() - 1; i++) {
        if ((i % 2) != 0) continue;
        m_file_hash.insert(
            m_file_hash.end(),
            hex_string_to_bytes.begin() + i,
            hex_string_to_bytes.begin() + i + 1
        );
    }
    
    if (resource != resource_type::urn)
        return std::unexpected("invalid file hash: did not get URN resource");
    if (protocol == protocol_type::unknown)
        return std::unexpected("invalid file hash: unknown file hash protocol");
    if (m_file_hash.size() != 20)
        return std::unexpected("invalid file hash: file hash length is not 20 bytes");

    return this;
}

const std::expected<torr::magnet*, const char*>
    torr::magnet::url_to_piece_hash(const std::string& url)
{
    http request;
    TRY(request.from_string(url));
    auto http_data = TRY(request.get_request());
    m_torrent_bencode.from_buffer(http_data);
    return this;
}

const std::expected<torr::magnet*, const char*>
    torr::magnet::from_string(const std::string& uri_string)
{
    if (uri_string.compare(0, 7, "magnet:") != 0)
        return std::unexpected("invalid magnet uri: missing 'magnet:' protocol");

    std::string regex_string = uri_string;
    std::smatch regex_matches;
    const std::regex regex_pattern(R"(..=([^&|\n|\t\s]+))");

    while (std::regex_search(regex_string, regex_matches, regex_pattern)) {
        if (regex_matches.size() <= 0) {
            regex_string = regex_matches.suffix().str();
            continue;
        }

        std::string param = regex_matches[0].str().substr(0, 2);

        if (param == "tr") {
            tracker tr;
            tr.set_string(regex_matches[1]);
            m_trackers.push_back(tr);
        }

        else if (param == "xt")
            TRY(urn_to_file_hash(regex_matches[1]));
        else if (param == "xs")
            TRY(url_to_piece_hash(regex_matches[1]));
        else if (param == "dn")
            m_file_name = regex_matches[1];

        regex_string = regex_matches.suffix().str();
    }

    if (m_trackers.size() <= 0)
        return std::unexpected("invalid magnet uri: missing trackers");
    if (m_file_hash.size() <= 0)
        return std::unexpected("invalid magnet uri: missing file hash");
    if (!validate_torrent_bencode_map(m_torrent_bencode))
        return std::unexpected("invalid magnet uri: missing or invalid bencode information");

    m_piece_length = m_torrent_bencode["info"]["piece length"].as_int();
    return this;
}
