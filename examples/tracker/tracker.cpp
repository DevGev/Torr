#include <generic/try.hpp>
#include <network/tracker.hpp>
#include <network/endpoint.hpp>
#include <network/peer/peer.hpp>
#include <uri/magnet.hpp>
#include <uri/url.hpp>
#include <multiproc/multiproc.hpp>
#include <multiproc/sandbox.h>
#include <torrent_file.hpp>
#include <cassert>
#include <print>

int main()
{
    torr::peer ourself;
    assert(
        ourself.randomize_identifier() == 20 &&
        "failed due to peer.randomize_identifier() != 20"
    );

    torr::torrent_file torrent;
    MUST(torrent.from_path("a.torrent"));
    ourself.set_download_target(torrent);

    auto first_tracker = torrent.trackers().at(0);
    auto announcer = MUST(first_tracker.announce(ourself));

    torr::torrent_peer peer;
    for (auto& address : announcer->peers()) {
        peer.set_ip_and_port(address.address, address.port);
        std::println("peer {}:{}", peer.ip_address_as_string(), peer.port());

        if (peer.handshake(ourself) == true)
            for (;;) peer.receive_message(ourself);
    }


    return 0;
}
