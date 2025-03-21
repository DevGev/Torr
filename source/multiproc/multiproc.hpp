#pragma once

#include <torrent.hpp>
#include <ipc/ipc.hpp>
#include <network/peer/peer.hpp>
#include <network/tracker.hpp>
#include <semaphore.h>
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
    sem_t* m_main_channel_mutex;
    torrent_peer m_peer;
    peer& m_ourself;

    void notify_downloaded_piece();

public:
    multiproc_task(peer&, const torrent_peer&);
    ~multiproc_task();

    void sandbox();
    bool work();
    void quit();
};

class multiproc {
private:
    ipc_channel m_main_channel;
    ipc_shared_memory m_bitfield_pieces;
    sem_t* m_main_channel_mutex;

    std::vector<peer_ip_touple> m_addresses;
    std::vector<pid_t> m_tasks;
    uint8_t m_spawn_children_count { 5 };
    bool m_is_spawning { false };

    peer& m_ourself;
    tracker& m_tracker;

    pid_t spawn();
    void spawner();
    void respawner();
    void handle_downloaded_piece(const multiproc_message& message);

public:
    multiproc(peer& ourself, tracker& track);
    ~multiproc();

    void start();
    void set_children_count(uint8_t count);
};

#define SANDBOX_FAILED() { \
        assert(false && "multiproc_task: sandbox failed"); \
        exit(0); \
        for (;;); }

}
