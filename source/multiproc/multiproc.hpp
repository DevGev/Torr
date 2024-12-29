#pragma once

#include <torrent.hpp>
#include <ipc/ipc.hpp>
#include <network/peer/peer.hpp>
#include <network/tracker.hpp>
#include <vector>

namespace torr {

enum class multiproc_message_type {
    unkown = 0,
    download_piece_done = 1,
};

struct multiproc_message {
    multiproc_message_type type;
    size_t payload_size;
    size_t field0;
};

class multiproc_task {
private:
    ipc_channel m_main_channel;
    ipc_shared_memory m_bitfield_pieces;
    torrent_peer m_peer;
    peer& m_ourself;

    void notify_downloaded_piece();

public:
    multiproc_task(peer&, const torrent_peer&);
    ~multiproc_task();

    bool work();
    void quit();
};

class multiproc {
private:
    ipc_channel m_main_channel;
    ipc_shared_memory m_bitfield_pieces;
    std::vector<peer_ip_touple> m_addresses;
    uint8_t m_spawn_children_count { 5 };

    peer& m_ourself;
    tracker& m_tracker;

    pid_t spawn();
    void handle_downloaded_piece(const multiproc_message& message);

public:
    multiproc(peer& ourself, tracker& track);
    ~multiproc();

    void start();
    
};

}
