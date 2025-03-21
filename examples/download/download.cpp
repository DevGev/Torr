#include <network/tracker.hpp>
#include <network/endpoint.hpp>
#include <network/peer/peer.hpp>
#include <uri/magnet.hpp>
#include <torrent_file.hpp>
#include <uri/url.hpp>
#include <multiproc/multiproc.hpp>
#include <multiproc/sandbox.h>

#include <cassert>
#include <print>

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::println("expected argument");
        return 0;
    }

    torr::peer ourself;
    assert(
        ourself.randomize_identifier() == 20 &&
        "failed due to peer.randomize_identifier() != 20"
    );

    auto source = torr::torrent_file();
    assert(
        source.from_path(std::string(argv[1])).has_value() &&
        "failed due to source.from_string() throwing std::unexpected"
    );

    assert(
        !source.trackers().empty() &&
        "failed due to source.trackers().empty()"
    );

    ourself.set_download_target(source);

    auto first_tracker = source.trackers().at(0);
    torr::multiproc download_process(ourself, first_tracker);
    download_process.set_children_count(1);
    download_process.start();
    return 0;
}
