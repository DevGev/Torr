#include "torrent_file.hpp"
#include <openssl/sha.h>
#include <fstream>

std::unique_ptr<torr::torrent_source> torr::torrent_file::copy() const 
{
    return std::move(
        std::unique_ptr<torr::torrent_source>
        (new torrent_file(*this))
    );
}

std::optional<const std::vector<std::byte>*> torr::torrent_file::file_hash() const
{
    return &m_file_hash;
}

std::optional<const std::string*> torr::torrent_file::file_name() const 
{
    return &m_file_name;
}

std::optional<size_t> torr::torrent_file::piece_length() const 
{
    return m_piece_length;
}

torr::torrent_source::source_type torr::torrent_file::type() const 
{
    return torr::torrent_source::source_type::torrent_file;
}

const std::vector<torr::tracker>& torr::torrent_file::trackers() const
{
    return m_trackers;
}

const std::expected<torr::torrent_file*, const char*>
    torr::torrent_file::from_path(const std::filesystem::path& file_path)
{
    const auto iflags = std::ios::in | std::ios::binary;
    std::ifstream file(file_path, iflags); 
    if (!file) return std::unexpected("invalid torrent file: could not resolve file path");

    auto data = std::vector<uint8_t>(std::istreambuf_iterator<char>{file}, {});
    m_torrent_bencode.store_raw_bencode();
    m_torrent_bencode.from_buffer(
        std::span {(std::byte*)data.data(), data.size()}
    );

    if (!validate_torrent_bencode_map(m_torrent_bencode))
        return std::unexpected("invalid torrent file: missing bencode information");

    m_piece_length = m_torrent_bencode["info"]["piece length"].as_int();

    m_file_hash.resize(20);
    auto info_raw = m_torrent_bencode["info"].as_raw();
    /* FIXME: append byte 'd' but not 'e'? fix in bencoder */
    info_raw.insert(info_raw.begin(), (std::byte)'d');
    SHA1((uint8_t*)info_raw.data(), info_raw.size(), (uint8_t*)m_file_hash.data());

    /* TODO: parse multiple announcers */
    tracker tr;
    tr.set_string(m_torrent_bencode["announce"].as_str());
    m_trackers.push_back(tr);

    return this;
}
