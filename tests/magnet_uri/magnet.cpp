#include <uri/magnet.hpp>
#include <network/socket/http.hpp>
#include <cassert>
#include <print>

#define TEST_NAME "uri/magnet.cpp"
#define TEST_MAGNET_STRING "magnet:?xt=urn:btih:dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c&dn=Big+Buck+Bunny&tr=udp://explodie.org:6969&tr=udp://tracker.coppersurfer.tk:6969&tr=udp://tracker.empire-js.us:1337&tr=udp://tracker.leechers-paradise.org:6969&tr=udp://tracker.opentrackr.org:1337&tr=wss://tracker.btorrent.xyz&tr=wss://tracker.fastcast.nz&tr=wss://tracker.openwebtorrent.com&ws=https://webtorrent.io/torrents/&xs=https://webtorrent.io/torrents/big-buck-bunny.torrent" 
#define EXPECTED_FILE_HASH "urn:btih:dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c"
#define EXPECTED_FILE_NAME "Big+Buck+Bunny"

int main()
{
    std::print("test: {} ... ", TEST_NAME);

    auto magnet = torr::magnet();

    assert(
        magnet.from_string(std::string(TEST_MAGNET_STRING)).has_value() &&
        "failed due to magnet.from_string() throwing std::unexpected"
    );

    assert(
        (unsigned char)magnet.file_hash().value()->at(19) == 0x1c &&
        "failed due to magnet.file_hash()[19]"
    );

    assert(
        (unsigned char)magnet.file_hash().value()->at(0) == 0xdd &&
        "failed due to magnet.file_hash()[0]"
    );

    assert(
        magnet.file_name().value()->compare(EXPECTED_FILE_NAME) == 0 &&
        "failed due to magnet.file_name()"
    );

    std::println("passed");
    return 0;
}
