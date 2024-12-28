#include "peer.hpp"

#include <fstream>
#include <random>
#include <print>
#include <climits>
#include <cstring>
#include <time.h>
#include <string.h>
#include <algorithm>
#include <functional>
#include <arpa/inet.h>

#define MAX_BYTES_IN_BITMAP 1024
#define HANDSHAKE_PREFIX \
    "\023BitTorrent protocol" \
    "\x00\x00\x00\x00\x00\x00\x00"

torr::peer::peer() {}
torr::peer::~peer() {}

size_t torr::peer::randomize_identifier()
{
    m_identifier.clear();
    srand(time(0));
    for (int i = 0; i < 20; i++)
        m_identifier.push_back((std::byte)(rand() % 255));
    return 20;
}

size_t torr::peer::construct_handshake_string()
{
    if (!m_download_target
        || m_download_target->file_hash().value()->size() <= 0
        || m_identifier.size() <= 0)
            return 0;

    size_t file_hash_length = m_download_target->file_hash().value()->size();
    size_t peer_id_length = m_identifier.size();
    size_t total_length = file_hash_length + peer_id_length + 28;
    if (total_length != 68)
        return 0;
    
    char handshake_prefix_cstr[] = HANDSHAKE_PREFIX;
    std::vector<std::byte> handshake_prefix(
        reinterpret_cast<std::byte*>(std::begin(handshake_prefix_cstr)), 
        reinterpret_cast<std::byte*>(std::end(handshake_prefix_cstr))
    );

    m_handshake.insert(
        m_handshake.end(),
        handshake_prefix.begin(),
        handshake_prefix.end()
    );

    m_handshake.insert(
        m_handshake.end(),
        m_download_target->file_hash().value()->begin(),
        m_download_target->file_hash().value()->end()
    );

    m_handshake.insert(
        m_handshake.end(),
        m_identifier.begin(),
        m_identifier.end()
    );

    std::println("handshake length {} ", m_handshake.size());
    return m_handshake.size();
}

void torr::peer::piece_download_complete(size_t piece_index,
    std::span<std::byte> piece_data)
{
    m_bitfield_pieces.bit_set(piece_index);
}

const std::vector<std::byte>& torr::peer::identifier() const
{
    return m_identifier;
}

const torr::torrent_source& torr::peer::download_target() const
{
    return *m_download_target;
}

const std::vector<std::byte>& torr::peer::handshake() const
{
    return m_handshake;
}

const dynamic_bitset& torr::peer::bitfield_pieces() const
{
    return m_bitfield_pieces;
}

void torr::peer::set_download_target(const torrent_source& ts)
{
    m_download_target = ts.copy();
    m_bitfield_pieces.resize(1024); /* FIXME: how many pieces in file actually */
}

torr::torrent_peer::torrent_peer()
{
}

torr::torrent_peer::~torrent_peer()
{
}

bool torr::torrent_peer::set_ip_and_port(const in_addr& ip, size_t& port)
{
    m_ip_address = ip;
    m_port = port;
    const char* ntoa = inet_ntoa(ip);
    if (!ntoa) return false;
    m_ip_address_string = ntoa; 
    return true;
}

bool torr::torrent_peer::handshake(peer& ourself)
{
    m_tcp.connect(m_ip_address, m_port);
    m_tcp.set_send_timeout(500000);

    if (ourself.handshake().empty() && !ourself.construct_handshake_string())
        return false;
    if (!m_tcp.send((uint8_t*)ourself.handshake().data(), ourself.handshake().size(), 0).has_value())
        return false;

    std::vector<std::byte> receive_handshake;
    size_t max_handshake_length = 68;
    receive_handshake.resize(max_handshake_length);
    if (!m_tcp.receive((uint8_t*)receive_handshake.data(), max_handshake_length))
        return false;

    for (const auto& e : receive_handshake)
        std::print("{}",(char)e);
    std::println();

    if (receive_handshake.size() < 68)
        return false;
    if (strncmp((char*)(receive_handshake.data()), HANDSHAKE_PREFIX, 28) != 0)
        return false;
    if (strncmp((char*)(receive_handshake.data() + 28),
        (char*)ourself.download_target().file_hash().value()->data(), 20) != 0)
        return false;

    m_handshake_complete = true;
    m_socket_healthy = true;
    std::println("handshake is complete!");

    return true;
}

bool torr::torrent_peer::receive_message(peer& ourself)
{
    if (!m_handshake_complete)
        return false;

    peer::message message;
    if (!m_tcp.receive((uint8_t*)&message, sizeof(peer::message)))
        return false;

    std::println("recv message {}", (int)message.type);

    switch (message.type) {
    case peer::message_type::choke:
        m_am_choking = 1;
        break;

    case peer::message_type::unchoke:
        receive_message_unchoke(ourself);
        break;

    case peer::message_type::interested:
        m_peer_intersted = 1;
        break;

    case peer::message_type::not_interested:
        m_peer_intersted = 0;
        break;

    case peer::message_type::have:
        receive_message_have(message);
        break;

    case peer::message_type::bitfield:
        receive_message_bitfield(message);
        break;

    case peer::message_type::request:
        receive_message_request(message);
        break;

    case peer::message_type::block:
        receive_message_block(ourself, message);
        break;

    case peer::message_type::cancel:
        receive_message_cancel(message);
        break;

    default:
        m_socket_healthy = receive_message_keep_alive(message);
        break;
    }

    return 0;
}

void torr::torrent_peer::determine_outstanding_requests(const peer& ourself)
{
    /* TODO: m_outstanding_requests = download_rate * queue_time */
    m_outstanding_requests = 90;
}

bool torr::torrent_peer::determine_download_piece(const peer& ourself)
{
    std::optional<size_t> found =
        ourself.bitfield_pieces().find_first_positive_bit_compliment(m_bitfield);
    if (!found.has_value())
        return false;

    size_t index_to_download = found.value();
    memset(&m_download_piece, 0, sizeof(m_download_piece));
    m_download_piece.piece_index = index_to_download;
    m_download_piece.piece_size = ourself.download_target().piece_length().value();

    std::println("decided on download piece at index {}", index_to_download);
    return true;
}

bool torr::torrent_peer::receive_message_unchoke(peer& ourself)
{
    std::println("receive unchoke!");
    m_am_choking = 0;

    if (!determine_download_piece(ourself))
        return false;

    determine_outstanding_requests(ourself);
    fill_outstanding_requests(ourself);
    return true;
}

bool torr::torrent_peer::receive_message_have(const peer::message& message)
{
    std::println("receive message have type={} length={}", (int)message.type, message.length.as_small_endian());

    if (message.length.as_small_endian() != sizeof(uint32_t) + 1)
        return false;

    uint32_t has_piece_index;
    if (!m_tcp.receive((uint8_t*)&has_piece_index, sizeof(uint32_t)))
        return false;

    m_bitfield.bit_set(has_piece_index);
    return false;
}

bool torr::torrent_peer::receive_message_bitfield(const peer::message& message)
{
    std::println("receive message bitfield type={} length={}", (int)message.type, message.length.as_small_endian());

    size_t bitfield_size = message.length.as_small_endian() - 1;
    m_bitfield.resize(bitfield_size);
    if (!m_tcp.receive((uint8_t*)m_bitfield.data(), bitfield_size))
        return false;

    std::println("sending message interested");
    return send_message_interested();
}

bool torr::torrent_peer::receive_message_request(const peer::message& message)
{
    std::println("receive message request type={} length={}", (int)message.type, message.length.as_small_endian());

    struct block_payload {
        big_endian_uint32_t block_index;
        big_endian_uint32_t block_offset;
        big_endian_uint32_t block_length;
    } __attribute__((packed));

    block_payload payload;

    if (message.length.as_small_endian() - 1 != sizeof(payload))
        return false;

    std::println("a peer wants block=%d, off=%d, length=%d",
        payload.block_index.as_small_endian(), payload.block_offset.as_small_endian(), payload.block_length.as_small_endian());

    /* TODO */
    std::println("TODO request not impl");

    return false;
}

bool torr::torrent_peer::receive_message_block(peer& ourself,
    const peer::message& message)
{
    std::println("receive message block type={} length={}", (int)message.type, message.length.as_small_endian());

    struct block_payload {
        big_endian_uint32_t block_index;
        big_endian_uint32_t block_offset;
    } __attribute__((packed));

    block_payload payload;
    size_t packet_size = message.length.as_small_endian() - 1;
    size_t block_length = packet_size - sizeof(block_payload);

    if (!m_tcp.receive((uint8_t*)&payload, sizeof(block_payload)))
        return false;

    std::println("piece={} block index={} offset={} length={}",
        m_download_piece.piece_index,
        payload.block_index.as_small_endian(),
        payload.block_offset.as_small_endian(),
        block_length
    );

    if (payload.block_index.as_small_endian() != m_download_piece.piece_index)
        return false;

    m_download_piece.data.resize(m_download_piece.downloaded + block_length);

    size_t packet_offset = 0;
    while (packet_offset < block_length) {
        uint8_t* download_buffer = (uint8_t*)m_download_piece.data.data() + m_download_piece.downloaded + packet_offset;
        auto receive_or_error = m_tcp.receive(download_buffer, block_length - packet_offset);
        if (!receive_or_error.has_value())
            return false;
        packet_offset += receive_or_error.value();
    }

    m_download_piece.downloaded += block_length;

    if (m_download_piece.downloaded >= m_download_piece.piece_size) {
        ourself.piece_download_complete(
            m_download_piece.piece_index,
            m_download_piece.data
        );

        send_message_have(m_download_piece);
        if (determine_download_piece(ourself))
            send_message_interested();
    }

    return true;
}

bool torr::torrent_peer::receive_message_cancel(const peer::message& message)
{
    /* TODO */
    std::println("TODO cancel not impl");

    return false;
}

bool torr::torrent_peer::receive_message_keep_alive(const peer::message& message)
{
    if (message.length.as_small_endian() != 0)
        return false;

    /* send keep-alive back */
    if (!m_tcp.send((uint8_t*)&message, sizeof(peer::message)))
        return false;
    return true;
}

bool torr::torrent_peer::fill_outstanding_requests(const peer& ourself)
{
    std::println("filling outstanding_ requests!");
    uint32_t outstanding_requests =
        ourself.download_target().piece_length().value() /
        MAX_BLOCK_SIZE;

    for (uint32_t i = 0; i < outstanding_requests; ++i)
        send_message_request(MAX_BLOCK_SIZE * i);
    return true;
}

bool torr::torrent_peer::send_message_request(uint32_t offset)
{
    struct request_payload {
        peer::message message;
        big_endian_uint32_t piece_index;
        big_endian_uint32_t begin;
        big_endian_uint32_t length;
    } __attribute__((packed));

    request_payload payload;
    payload.piece_index = m_download_piece.piece_index;
    payload.begin = m_download_piece.downloaded + offset;
    payload.length = MAX_BLOCK_SIZE;
    payload.message.length = sizeof(payload) - sizeof(uint32_t);
    payload.message.type = peer::message_type::request;

    std::println("request length={} offset={} piece_index={}",
        payload.message.length.as_small_endian(),
        payload.begin.as_small_endian(),
        m_download_piece.piece_index);

    if (!m_tcp.send((uint8_t*)&payload, sizeof(payload)))
        return false;

    std::println("sent message request succesfully");
    return true;
}

bool torr::torrent_peer::send_message_interested()
{
    peer::message message;
    message.length = 1;
    message.type = peer::message_type::interested;
    size_t total_size = sizeof(message);
    if (!m_tcp.send((uint8_t*)&message, sizeof(message)))
        return false;

    std::println("sent message is_interested succesfully");
    return true;
}

bool torr::torrent_peer::send_message_bitfield(const peer& ourself)
{
    peer::message message;
    message.length = 1 + ourself.bitfield_pieces().bytes_size();
    message.type = peer::message_type::bitfield;
    const uint8_t* data = ourself.bitfield_pieces().const_data();

    if (!m_tcp.send((uint8_t*)&message, sizeof(message)) ||
        !m_tcp.send(data, ourself.bitfield_pieces().bytes_size()))
        return false;
    return true;
}


bool torr::torrent_peer::send_message_have(const download_torrent_piece& piece)
{
    struct have_payload {
        peer::message message;
        big_endian_uint32_t piece_index;
    } __attribute__((packed));

    have_payload payload;
    payload.message.type = peer::message_type::have;
    payload.message.length = sizeof(payload) - sizeof(uint32_t);
    payload.piece_index = piece.piece_index;

    if (!m_tcp.send((uint8_t*)&payload, sizeof(payload)))
        return false;

    std::println("sent message have {} succesfully", piece.piece_index);
    return true;
}

const std::string& torr::torrent_peer::ip_address_as_string() const
{
    return m_ip_address_string;
}

const size_t torr::torrent_peer::port() const
{
    return m_port;
}
