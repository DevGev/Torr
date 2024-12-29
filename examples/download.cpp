#include <network/tracker.hpp>
#include <network/endpoint.hpp>
#include <network/peer/peer.hpp>
#include <uri/magnet.hpp>
#include <uri/url.hpp>
#include <multiproc/multiproc.hpp>

#include <cassert>
#include <print>

#define MAGNET "magnet:?xt=urn:btih:dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c&dn=Big+Buck+Bunny&tr=udp://explodie.org:6969&tr=udp://tracker.coppersurfer.tk:6969&tr=udp://tracker.empire-js.us:1337&tr=udp://tracker.leechers-paradise.org:6969&tr=udp://tracker.opentrackr.org:1337&tr=wss://tracker.btorrent.xyz&tr=wss://tracker.fastcast.nz&tr=wss://tracker.openwebtorrent.com&ws=https://webtorrent.io/torrents/&xs=https://webtorrent.io/torrents/big-buck-bunny.torrent" 

int single_process_main()
{
    torr::peer ourself;
    assert(
        ourself.randomize_identifier() == 20 &&
        "failed due to peer.randomize_identifier() != 20"
    );

    auto magnet = torr::magnet();
    assert(
        magnet.from_string(std::string(MAGNET)).has_value() &&
        "failed due to magnet.from_string() throwing std::unexpected"
    );

    assert(
        !magnet.trackers().empty() &&
        "failed due to magnet.trackers().empty()"
    );

    ourself.set_download_target(magnet);

    auto first_tracker = magnet.trackers().at(0);
    auto announcer_or_error = first_tracker.announce(ourself);

    assert(
        announcer_or_error.has_value() &&
        "failed due to tracker.announce()"
    );

    torr::torrent_peer peer;
    for (auto& address : announcer_or_error.value()->peers()) {
        peer.set_ip_and_port(address.address, address.port);
        std::println("peer {}:{}", peer.ip_address_as_string(), peer.port());

        if (peer.handshake(ourself) == true)
            for (;;) peer.receive_message(ourself);
    }

    return 0;
}

int main()
{
    std::print("test: multiproc network ... ");

    torr::peer ourself;
    assert(
        ourself.randomize_identifier() == 20 &&
        "failed due to peer.randomize_identifier() != 20"
    );

    auto magnet = torr::magnet();
    assert(
        magnet.from_string(std::string(TEST_MAGNET_STRING)).has_value() &&
        "failed due to magnet.from_string() throwing std::unexpected"
    );

    assert(
        !magnet.trackers().empty() &&
        "failed due to magnet.trackers().empty()"
    );

    ourself.set_download_target(magnet);

    auto first_tracker = magnet.trackers().at(0);
    torr::multiproc download_process(ourself, first_tracker);
    download_process.start();
    return 0;
}
