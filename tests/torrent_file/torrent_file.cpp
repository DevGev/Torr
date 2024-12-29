#include <torrent_file.hpp>
#include <torrent.hpp>
#include <print>

#define TEST_NAME "torrent_file.cpp"
#define TEST_FILE "torrent_file/test.torrent"
#define TEST_EXPECTED_INFO_HASH "dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c"
#define TEST_EXPECTED_PIECE_LENGTH 262144

int main()
{
    std::print("test: {} ... ", TEST_NAME);

    torr::torrent_file file;

    assert(
        file.from_path(TEST_FILE).has_value() &&
        "failed due to torrent_file.from_path() throwing std::unexpected"
    );

    auto info_hash = *file.file_hash().value();
    std::string expected_info_hash = TEST_EXPECTED_INFO_HASH;
    std::string out_info_hash;

    for (const auto& e : info_hash) {
        char out[3];
        snprintf(out, 3, "%x", (uint8_t)e);
        out_info_hash += out[0];
        out_info_hash += out[1];
    }

    assert(
        expected_info_hash.compare(out_info_hash) == 0 &&
        "failed due to info hash not matching expected info hash"
    );

    assert(
        file.piece_length() == TEST_EXPECTED_PIECE_LENGTH &&
        "failed due to piece length not matching expected piece length"
    );

    assert(
        !file.trackers().empty() &&
        "failed due to missing tracker"
    );

    std::println("passed");

    return 0;
}
