#pragma once

#include <torrent.hpp>
#include <network/socket/endian.hpp>
#include <network/socket/udp.hpp>
#include <network/endpoint.hpp>
#include <network/peer/peer.hpp>
#include <uri/url.hpp>
#include <expected>
#include <vector>

#define MAX_PEERS 1024

namespace torr {

class tracker;

class tracker_response {
private:
    std::vector<peer_ip_touple> m_peer_addresses;
    size_t m_interval {};
    size_t m_leechers {};
    size_t m_seeders {};

public:
    tracker_response() {}
    ~tracker_response() {}

    void set_interval(const size_t& interval);
    void set_leechers_seeders(const size_t& leechers, const size_t& seeders);
    void push_peer(const torrent_peer& p);
    void clear_peers();

    const std::vector<peer_ip_touple>& peers() { return m_peer_addresses; };
    const size_t& interval() const { return m_interval; };
    const size_t& leechers() const { return m_leechers; };
    const size_t& seeders() const { return m_seeders; };

    friend tracker;
};

class tracker {
public:

private:
    url m_tracker_as_url;
    tracker_response m_tracker_response;
    endpoint m_endpoint {};

    bool m_has_endpoint { false };
    std::string m_tracker_as_string;

    uint32_t m_announce_transaction_identifier {};
    uint64_t m_announce_connection_identifier {};

    std::expected<tracker_response*, const char*>
        http_announce(const peer& ourself);

    std::expected<tracker_response*, const char*>
        udp_announce(const peer& ourself);

    std::expected<size_t, const char*>
        udp_announce_transaction(const peer& ourself, const udp& udp_socket);

    std::expected<size_t, const char*>
        udp_connetion_transaction(const peer& ourself, const udp& udp_socket);

    std::expected<size_t, const char*>
        udp_scrape_transaction(const peer& ourself, const udp& udp_socket);

public:
    tracker();
    ~tracker();

    std::expected<endpoint, const char*>
        string_to_endpoint(const std::string &);

    std::expected<tracker_response*, const char*>
        announce(const peer& ourself);

    const std::string& as_string() const;
    void set_string(const std::string&);

private:

    /* UDP tracker protocol structures
     * reference:
     * https://xbtt.sourceforge.net/udp_tracker_protocol.html */
    struct udp_announce_connection_input {
        big_endian_uint64_t connection_identifier { 0x41727101980 };
        big_endian_uint32_t action { 0 };
        big_endian_uint32_t transaction_identifier { 0 };
    } __attribute__((packed));

    struct udp_announce_connection_output {
        big_endian_uint32_t action { 0 };
        big_endian_uint32_t transaction_identifier { 0 };
        big_endian_uint64_t connection_identifier { 0 };
    } __attribute__((packed));

    struct udp_announce_input {
        udp_announce_connection_input
            connection_input { 0, 1, 0 };
        uint8_t info_hash[20];
        uint8_t peer_id[20];
        big_endian_uint64_t downloaded_size { 0 };
        big_endian_uint64_t download_size_left { 0 };
        big_endian_uint64_t uploaded_size { 0 };
        big_endian_uint32_t event { 0 };
        big_endian_uint32_t ip_address { 0 };
        big_endian_uint32_t key { 0 };
        big_endian_uint32_t number_wait { (uint32_t)-1 };
        big_endian_uint16_t port { 8000 };
    } __attribute__((packed));

    struct udp_announce_ip_and_port {
        big_endian_uint32_t ip_address { 0 };
        big_endian_uint16_t tcp_port { 0 };
    } __attribute((packed));

    const size_t minimum_udp_announce_output_size { 20 };
    struct udp_announce_output {
        big_endian_uint32_t action { 1 };
        big_endian_uint32_t transaction_identifier { 0 };
        big_endian_uint32_t interval { 0 };
        big_endian_uint32_t leechers { 0 };
        big_endian_uint32_t seeders { 0 };
        udp_announce_ip_and_port peers[MAX_PEERS];
    } __attribute__((packed));

    struct udp_announce_scrape_input {
        big_endian_uint64_t connection_identifier { 0 };
        big_endian_uint32_t action { 2 };
        big_endian_uint32_t transaction_identifier { 0 };
        uint8_t info_hash[20];
    } __attribute__((packed));

    struct udp_scrape_output_list {
        big_endian_uint32_t seeder;
        big_endian_uint32_t completed;
        big_endian_uint32_t leecher;
    } __attribute((packed));

    const size_t minimum_udp_scrape_output_size { 8 };
    struct udp_announce_scrape_output {
        big_endian_uint32_t action { 2 };
        big_endian_uint32_t transaction_identifier { 0 };
        udp_scrape_output_list scraped[MAX_PEERS];
    } __attribute__((packed));

    struct udp_announce_error {
        big_endian_uint32_t action { 3 };
        big_endian_uint32_t transaction_identifier { 0 };
        char message[];
    } __attribute__((packed));
};

}
