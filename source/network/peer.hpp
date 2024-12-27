#pragma once

#include <torrent.hpp>
#include <ak/dynamic_bitset.hpp>
#include <network/socket/tcp.hpp>
#include <network/socket/endian.hpp>
#include <network/endpoint.hpp>
#include <bitset>
#include <vector>

#define MAX_BITFIELD_BYTES 512
#define MAX_BLOCK_SIZE 16384
#define MAX_BLOCKS_IN_PIECE 1024

namespace torr {

class peer {

public:

enum class message_type :
    std::underlying_type_t<std::byte>
{
    choke = 0,
    unchoke = 1,
    interested = 2,
    not_interested = 3,
    have = 4,
    bitfield = 5,
    request = 6,
    block = 7,
    cancel = 8,
};

struct message {
    big_endian_uint32_t length = 0;
    message_type type {};
} __attribute__((packed));

private:
    std::unique_ptr<torrent_source> m_download_target;
    std::vector<std::byte> m_identifier;
    std::vector<std::byte> m_handshake;
    dynamic_bitset m_bitfield_pieces;
    endpoint m_endpoint;

public:
    peer();
    ~peer();

    size_t randomize_identifier();
    size_t construct_handshake_string();
    void set_download_target(const torrent_source&);

    const std::vector<std::byte>& identifier() const;
    const torrent_source& download_target() const;
    const std::vector<std::byte>& handshake() const;
    const dynamic_bitset& bitfield_pieces() const;
};

class torrent_peer {
public:
    struct download_torrent_piece {
        size_t piece_index {};
        size_t piece_size {};
        size_t downloaded {};
        std::vector<std::byte> data;
    };

private:
    tcp m_tcp;
    dynamic_bitset m_bitfield;
    download_torrent_piece m_download_piece;

    std::string m_ip_address_string;
    bool m_socket_healthy {};
    in_addr m_ip_address {};
    size_t m_port {};
    uint32_t m_outstanding_requests {};

    bool m_handshake_complete { 0 };
    bool m_am_interested { 0 };
    bool m_am_choking { 1 };
    bool m_peer_intersted { 0 };
    bool m_peer_choking { 1 };
    bool m_can_send_request { 1 };

    void determine_outstanding_requests(const peer& ourself);
    bool determine_download_piece(const peer& ourself);

    bool receive_message_unchoke(peer& ourself);
    bool receive_message_cancel(const peer::message& message);
    bool receive_message_block(const peer::message& message);
    bool receive_message_request(const peer::message& message);
    bool receive_message_bitfield(const peer::message& message);
    bool receive_message_have(const peer::message& message);
    bool receive_message_keep_alive(const peer::message& message);

    bool fill_outstanding_requests(const peer& ourself);
    bool send_message_request(uint32_t offset = 0);
    bool send_message_interested();
    bool send_message_bitfield(const peer& ourself);

public:
    torrent_peer();
    ~torrent_peer();

    bool receive_message(peer& ourself);
    bool set_ip_and_port(const in_addr&, size_t&);
    bool handshake(peer& ourself);
    const std::string& ip_address_as_string() const;
    const size_t port() const;
};

}
