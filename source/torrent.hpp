#pragma once

#include <generic/bencode_map.hpp>
#include <filesystem> 
#include <optional>
#include <vector>
#include <memory>
#include <string>
#include <cassert>
#include <arpa/inet.h>

#define VIRTUAL_MEMBER_FUNCTION return {};

namespace torr {

struct peer_ip_touple {
    in_addr address;
    size_t port;
} __attribute__ ((packed));

class torrent_source {
public:
    enum class source_type {
        unknown,
        magnet_uri,
        torrent_file,
    };

    virtual std::unique_ptr<torrent_source> copy() const
        { VIRTUAL_MEMBER_FUNCTION };
    virtual std::optional<const std::vector<std::byte>*> file_hash() const
        { VIRTUAL_MEMBER_FUNCTION };
    virtual std::optional<const std::string*> file_name() const
        { VIRTUAL_MEMBER_FUNCTION };
    virtual std::optional<size_t> piece_length() const
        { VIRTUAL_MEMBER_FUNCTION };
    virtual source_type type() const
        { VIRTUAL_MEMBER_FUNCTION };
};

inline bool validate_torrent_bencode_map(bencode_map& bencode)
{
    if (bencode["announce"].type() != bencode_map::target_type::strings)
        return false;
    if (bencode["info"]["piece length"].type() != bencode_map::target_type::integers)
        return false;
    if (bencode["info"]["pieces"].type() != bencode_map::target_type::strings)
        return false;
    return true;
}

}
