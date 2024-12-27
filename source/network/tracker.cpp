#include "tracker.hpp"
#include "socket/udp.hpp"
#include <ak/dynamic_bitset.hpp>
#include <utility>
#include <cassert>
#include <print>
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

    auto url_or_error = m_tracker_as_url.from_string(tracker_as_string);
    if (!url_or_error.has_value())
        return std::unexpected(url_or_error.error());

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
    if (!m_has_endpoint) {
        auto endpoint_or_error = string_to_endpoint(m_tracker_as_string);
        if (!endpoint_or_error.has_value())
            return std::unexpected("invalid tracker: failed to resolve endpoint");
    }

    switch (m_endpoint.m_protocol) {
    case url::protocol_type::https:
        return https_announce(ourself);
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
    auto send_or_error = udp_socket.send((uint8_t*)&connection_input, plen);
    if (!send_or_error.has_value())
        return std::unexpected(send_or_error.error());
    if (send_or_error.value_or(0) < plen)
        return std::unexpected("udp tracker connection transaction: udp send failed");

    plen = sizeof(udp_announce_connection_output);
    udp_announce_connection_output connection_output;

    /* 1. Receive the packet. */
    /* 2. Check whether the packet is at least 16 bytes. */
    if (udp_socket.receive((uint8_t*)&connection_output, plen).value_or(0) < plen)
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
    auto send_or_error = udp_socket.send((uint8_t*)&announce_input, plen);
    if (!send_or_error.has_value())
        return std::unexpected(send_or_error.error());
    if (send_or_error.value_or(0) < plen)
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

        torrent_peer tp;
        tp.set_ip_and_port(ip_addr, port);
        m_tracker_response.push_peer(tp);
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
    auto send_or_error = udp_socket.send((uint8_t*)&scrape_input, plen);
    if (!send_or_error.has_value())
        return std::unexpected(send_or_error.error());
    if (send_or_error.value_or(0) < plen)
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
    auto udp_socket_error = udp_socket.connect(m_endpoint.m_ip_address, m_endpoint.m_port);
    if (!udp_socket_error.has_value())
        return std::unexpected(udp_socket_error.error());
    
    auto connection_transaction_or_error =
        udp_connetion_transaction(ourself, udp_socket);
    if (!connection_transaction_or_error.has_value())
        return std::unexpected(connection_transaction_or_error.error());

    auto announce_transaction_or_error =
        udp_announce_transaction(ourself, udp_socket);
    if (!announce_transaction_or_error.has_value())
        return std::unexpected(announce_transaction_or_error.error());

    return &m_tracker_response;
}

std::expected<torr::tracker_response*, const char*>
    torr::tracker::http_announce(const peer& ourself)
{
    assert(1 == 2 && "http_announce() is not implemented");
    std::unreachable();
}

std::expected<torr::tracker_response*, const char*>
    torr::tracker::https_announce(const peer& ourself)
{
    assert(1 == 2 && "https_announce() is not implemented");
    std::unreachable();
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

void torr::tracker_response::push_peer(const torrent_peer& p)
{
    m_peers.emplace_back(p);
}

void torr::tracker_response::clear_peers()
{
    m_peers.clear();
}
