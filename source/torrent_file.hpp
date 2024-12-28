#pragma once

#include <generic/bencode_map.hpp>
#include <network/tracker.hpp>
#include <torrent.hpp>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <expected>

namespace torr {

class torrent_file : public torrent_source {
private:
    bencode_map m_torrent_bencode;
    std::vector<tracker> m_trackers;
    std::vector<std::byte> m_file_hash;
    std::string m_file_name;
    size_t m_piece_length = 0;

public:
    torrent_file() {}
    ~torrent_file() {}

    std::unique_ptr<torrent_source> copy() const override;
    std::optional<const std::vector<std::byte>*> file_hash() const override;
    std::optional<const std::string*> file_name() const override;
    std::optional<size_t> piece_length() const override;
    source_type type() const override;

    const std::vector<tracker>& trackers() const;
    const std::expected<torrent_file, const char*> 
        from_path(const std::filesystem::path& file_path);
};

}
