#pragma once

#include <generic/bencode_map.hpp>
#include <network/tracker.hpp>
#include <uri/url.hpp>
#include <torrent.hpp>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <expected>

namespace torr {

class magnet : public torrent_source {
private:
    std::vector<tracker> m_trackers;
    std::vector<std::byte> m_file_hash;
    std::string m_file_name;
    size_t m_piece_length;
    bencode_map m_torrent_bencode;

    const std::expected<magnet, const char*>
        urn_to_file_hash(const std::string&);
    const std::expected<magnet, const char*>
        url_to_piece_hash(const std::string&);

public:
    magnet();
    ~magnet();

    std::unique_ptr<torrent_source> copy() const override;
    std::optional<const std::vector<std::byte>*> file_hash() const override;
    std::optional<const std::string*> file_name() const override;
    std::optional<size_t> piece_length() const override;
    torrent_source::source_type type() const override;

    const std::vector<tracker>& trackers() const;
    const std::expected<magnet, const char*>
        from_string(const std::string&);
};

}
