#include "tracker.hpp"
#include "socket/udp.hpp"
#include "socket/http.hpp"
#include <generic/dynamic_bitset.hpp>
#include <generic/try.hpp>
#include <utility>
#include <cassert>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

torr::tracker::tracker() {}
torr::tracker::~tracker() {}

std::expected<torr::endpoint, const char*>
    torr::tracker::string_to_endpoint(const std::string& tracker_as_string)
{
    if (tracker_as_string.empty())
        return std::unexpected("invalid tracker: empty string");

    TRY(m_tracker_as_url.from_string(tracker_as_string));
    m_endpoint.m_port = m_tracker_as_url.port();
    m_endpoint.m_protocol = m_tracker_as_url.protocol();

    struct hostent* hostent = gethostbyname(m_tracker_as_url.host().c_str());
    if (hostent == NULL)
        return std::unexpected("invalid tracker: gethostbyname() failed");
    m_endpoint.m_ip_address.s_addr = ((struct in_addr*)hostent->h_addr)->s_addr;
    m_has_endpoint = true;
    m_tracker_as_string = tracker_as_string;
    return m_endpoint;
}

std::expected<torr::tracker_response*, const char*>
    torr::tracker::announce(const peer& ourself)
{
    if (!m_has_endpoint)
        TRY(string_to_endpoint(m_tracker_as_string));

    switch (m_endpoint.m_protocol) {
    case url::protocol_type::https:
        return http_announce(ourself);
    case url::protocol_type::http:
        return http_announce(ourself);
    case url::protocol_type::udp:
        return udp_announce(ourself);
    default:
        return std::unexpected(
            "announce: tracker protocol not implemented "
            "(supported protocols: http, https, udp)"
        );
    };

    std::unreachable();
}

std::expected<size_t, const char*>
    torr::tracker::udp_connetion_transaction(const peer& ourself, const torr::udp& udp_socket)
{
    /* 1. Choose a (random) transaction ID. */
    m_announce_transaction_identifier = time(NULL);

    /* 2. Fill the connect input structure. */
    size_t plen = sizeof(udp_announce_connection_input);
    udp_announce_connection_input connection_input;
    connection_input.transaction_identifier = m_announce_transaction_identifier;

    /* 3. Send the packet. */
    auto sent = TRY(udp_socket.send((uint8_t*)&connection_input, plen));
    if (sent < plen)
        return std::unexpected("udp tracker connection transaction: udp send failed");

    plen = sizeof(udp_announce_connection_output);
    udp_announce_connection_output connection_output;

    /* 1. Receive the packet. */
    /* 2. Check whether the packet is at least 16 bytes. */
    if (udp_socket.receive((uint8_t*)&connection_output, plen, 0, 5).value_or(0) < plen)
        return std::unexpected("udp tracker connection transaction: udp receive failed");

    /* 3. Check whether the transaction id is equal to the one you chose. */
    if (connection_output.transaction_identifier != m_announce_transaction_identifier)
        return std::unexpected("udp tracker connection transaction: connection transaction identifier mismatch");

    /* 4. Check whether the action is correct. */
    if (connection_output.action != 0)
        return std::unexpected("udp tracker connection transaction: connection action mismatch");

    /* 5. Store the connection id. */
    m_announce_connection_identifier = connection_output.connection_identifier;
    
    return m_announce_connection_identifier;
}

std::expected<size_t, const char*>
    torr::tracker::udp_announce_transaction(const peer& ourself, const torr::udp& udp_socket)
{
    /* 1. Choose a (random) transaction ID. */
    m_announce_transaction_identifier = time(NULL);

    /* 2. Fill the announce input structure. */
    size_t plen = sizeof(udp_announce_input);
    udp_announce_input announce_input;
    announce_input.connection_input.connection_identifier = m_announce_connection_identifier;
    announce_input.connection_input.transaction_identifier = m_announce_transaction_identifier;
    announce_input.connection_input.action = 1;
    strncpy((char*)announce_input.peer_id, (const char*)ourself.identifier().data(), 20);

    auto optional_info_hash = ourself.download_target().file_hash();
    if (!optional_info_hash.has_value())
        return std::unexpected("udp tracker scrape transaction: missing peer download target");
    strncpy((char*)announce_input.info_hash, (char*)optional_info_hash.value()->data(), 20);

    /* 3. Send the packet. */
    auto sent = TRY(udp_socket.send((uint8_t*)&announce_input, plen));
    if (sent < plen)
        return std::unexpected("udp tracker announce transaction: udp send failed");

    plen = sizeof(udp_announce_output);
    udp_announce_output announce_output;

    /* 1. Receive the packet. */
    /* 2. Check whether the packet is at least 20 bytes. */
    if (udp_socket.receive((uint8_t*)&announce_output, plen).value_or(0) < minimum_udp_announce_output_size)
        return std::unexpected("udp tracker announce transaction: udp receive failed");

    /* 3. Check whether the transaction ID is equal to the one you chose. */
    if (announce_output.transaction_identifier != m_announce_transaction_identifier)
        return std::unexpected("udp tracker announce transaction: connection transaction identifier mismatch");

    /* 4. Check whether the action is announce. */
    if (announce_output.action != 1)
        return std::unexpected("udp tracker announce transaction: announce action mismatch");

    /* 5. Do not announce again until interval seconds have passed or an event has happened. */
    /* ... */
    m_tracker_response.set_interval(announce_output.interval);
    m_tracker_response.clear_peers();

    for (size_t i = 0; i < MAX_PEERS; ++i) {
        if (!announce_output.peers[i].ip_address && !announce_output.peers[i].tcp_port)
            break;

        struct in_addr ip_addr;
        ip_addr.s_addr = ntohl(announce_output.peers[i].ip_address);
        size_t port = announce_output.peers[i].tcp_port;
        m_tracker_response.m_peer_addresses.push_back({ ip_addr, port });
    }

    return m_announce_connection_identifier;
}

std::expected<size_t, const char*>
    torr::tracker::udp_scrape_transaction(const peer& ourself, const torr::udp& udp_socket)
{
    /* 1. Choose a (random) transaction ID. */
    m_announce_transaction_identifier = time(NULL);

    /* 2. Fill the scrape input structure. */
    size_t plen = sizeof(udp_announce_scrape_input);
    udp_announce_scrape_input scrape_input;
    scrape_input.transaction_identifier = m_announce_transaction_identifier;
    scrape_input.connection_identifier = m_announce_connection_identifier;

    auto optional_info_hash = ourself.download_target().file_hash();
    if (!optional_info_hash.has_value())
        return std::unexpected("udp tracker scrape transaction: missing peer download target");
    strncpy((char*)scrape_input.info_hash, (char*)optional_info_hash.value()->data(), 20);

    /* 3. Send the packet. */
    auto sent = TRY(udp_socket.send((uint8_t*)&scrape_input, plen));
    if (sent < plen)
        return std::unexpected("udp tracker scrape transaction: udp send failed");

    /* 1. Receive the packet. */
    /* 2. Check whether the packet is at least 8 bytes. */
    plen = sizeof(udp_announce_scrape_output);
    udp_announce_scrape_output scrape_output;
    if (udp_socket.receive((uint8_t*)&scrape_output, plen).value_or(0) < minimum_udp_scrape_output_size)
        return std::unexpected("udp tracker scrape transaction: udp receive failed");

    /* Check whether the transaction ID is equal to the one you chose. */
    if (scrape_output.transaction_identifier != m_announce_transaction_identifier)
        return std::unexpected("udp tracker scrape transaction: connection transaction identifier mismatch");

    /* Check whether the action is scrape. */
    if (scrape_output.action != 2)
        return std::unexpected("udp tracker scrape transaction: announce action mismatch");

    return m_announce_connection_identifier;
}

std::expected<torr::tracker_response*, const char*>
    torr::tracker::udp_announce(const peer& ourself)
{
    udp udp_socket;
    TRY(udp_socket.connect(m_endpoint.m_ip_address, m_endpoint.m_port));
    TRY(udp_connetion_transaction(ourself, udp_socket));
    TRY(udp_announce_transaction(ourself, udp_socket));
    return &m_tracker_response;
}

std::expected<torr::tracker_response*, const char*>
    torr::tracker::http_announce(const peer& ourself)
{
    auto optional_info_hash = ourself.download_target().file_hash();
    if (!optional_info_hash.has_value())
        return std::unexpected("http tracker announce: missing peer download target");

    std::string url = m_tracker_as_string;
    url += "?info_hash=";
    url += url::bytes_as_url_escaped_string(std::span {
        optional_info_hash.value()->data(),
        optional_info_hash.value()->size()
    });

    url += "&peer_id=";
    url += url::bytes_as_url_escaped_string(std::span {
        ourself.identifier().data(),
        ourself.identifier().size(),
    });

    url += "&uploaded=0";
    url += "&downloaded=0";
    url += "&left=0";
    url += "&compact=1";
    url += "&port=6881";

    http request;
    bencode_map bencode;
    TRY(request.from_string(url));
    bencode.from_buffer(TRY(request.get_request()));

    if (bencode["peers"].type() != bencode_map::target_type::strings)
        return std::unexpected("http tracker announce: did not receive compact peer list response");
    std::string peers_string = bencode["peers"].as_str();

    udp_announce_ip_and_port peers[MAX_PEERS];
    memset(&peers, 0, sizeof(peers));
    memcpy(&peers, peers_string.c_str(), peers_string.size() < sizeof(peers)
        ? peers_string.size() : sizeof(peers));

    for (size_t i = 0; i < MAX_PEERS; ++i) {
        if (!peers[i].ip_address && !peers[i].tcp_port)
            break;

        struct in_addr ip_addr;
        ip_addr.s_addr = ntohl(peers[i].ip_address);
        size_t port = peers[i].tcp_port;
        m_tracker_response.m_peer_addresses.push_back({ ip_addr, port });
    }

    return &m_tracker_response;
}

const std::string& torr::tracker::as_string() const
{
    return m_tracker_as_string;
}

void torr::tracker::set_string(const std::string& s)
{
    m_tracker_as_string = s;
}

void torr::tracker_response::set_interval(const size_t& interval)
{
    m_interval = interval;
}

void torr::tracker_response::set_leechers_seeders(const size_t& leechers, const size_t& seeders)
{
    m_leechers = leechers; m_seeders = seeders;
}

void torr::tracker_response::clear_peers()
{
    m_peer_addresses.clear();
}
