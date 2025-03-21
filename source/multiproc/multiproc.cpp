#include <generic/try.hpp>
#include <multiproc/multiproc.hpp>
#include <multiproc/sandbox.h>
#include <thread>
#include <print>
#include <span>
#include <fstream>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory.h>

torr::multiproc_task::multiproc_task(peer& ourself, const torrent_peer& them)
    : m_bitfield_pieces("bitfield_pieces", 2048),
    m_ourself(ourself),
    m_peer(them)
{
    m_main_channel.set_pid(getppid());
    m_main_channel.connect_channel();
    m_main_channel_mutex = sem_open(
        "torr.main_channel_mutex", O_CREAT, 0644, 0);
    if (m_main_channel_mutex == SEM_FAILED) quit();
}

torr::multiproc_task::~multiproc_task() {}

torr::multiproc::multiproc(peer& ourself, tracker& track)
    : m_bitfield_pieces("bitfield_pieces", 2048),
    m_ourself(ourself),
    m_tracker(track)
{
    m_main_channel.create_channel();
    m_main_channel_mutex = sem_open(
        "torr.main_channel_mutex", O_CREAT, 0644, 0);
    if (m_main_channel_mutex == SEM_FAILED)
        assert(false && "sem_open() failed");
    sem_post(m_main_channel_mutex);

    ourself.set_shared_bitfield(
        (uint8_t*)m_bitfield_pieces.memory_pointer(),
        m_bitfield_pieces.capacity()
    );

    /* FIXME: pass multiple trackers */
    auto announcer = MUST(track.announce(m_ourself));
    m_addresses = announcer->peers();
}

torr::multiproc::~multiproc()
{
    sem_destroy(m_main_channel_mutex);
}

void torr::multiproc_task::quit()
{
    exit(0);
}

void torr::multiproc_task::sandbox()
{
    if (!sandbox_landlock_process())
        SANDBOX_FAILED();
    if (!sandbox_seccomp_filter_process())
        SANDBOX_FAILED();
}

bool torr::multiproc_task::work()
{
    for (;;) {
        m_peer.receive_message(m_ourself);

        const auto& piece = m_peer.download_piece();
        if (piece.downloaded && piece.downloaded >= piece.piece_size) {
            notify_downloaded_piece();
            m_peer.download_next_piece(m_ourself);
        }

        if (!m_peer.socket_healthy())
            quit();
    }
    return true;
}

void torr::multiproc_task::notify_downloaded_piece()
{
    std::span<std::byte> output_data = {
        (std::byte*)m_peer.download_piece().data.data(),
        m_peer.download_piece().data.capacity()
    };

    multiproc_message message;
    message.type = multiproc_message_type::download_piece_done;
    message.payload_size = output_data.size();
    message.field0 = m_peer.download_piece().piece_index;

    sem_wait(m_main_channel_mutex);
    m_main_channel.write({ (std::byte*)&message, sizeof(message) });
    m_main_channel.write(output_data);
    sem_post(m_main_channel_mutex);

    m_peer.empty_download_piece();
}

pid_t torr::multiproc::spawn()
{
    torrent_peer peer;
    bool has_found_peer = false;

    for (std::vector<peer_ip_touple>::iterator it = m_addresses.begin();
            it != m_addresses.end(); ) {
        const auto& address = *it;
        peer.set_ip_and_port(address.address, address.port);
        std::println("testing peer {}:{}", peer.ip_address_as_string(), peer.port());

        if (peer.handshake(m_ourself)) {
            has_found_peer = true;
            m_addresses.erase(it);
            break;
        }
        
        it = m_addresses.erase(it);
    }

    if (!has_found_peer)
        return 0;

    pid_t c_pid = fork(); 
    if (c_pid == -1) {
        /* FIXME: very unlikely yet possible,
         * fail gracefully? */
        assert(false);
        return -1;
    }
    else if (c_pid > 0) {
        m_tasks.push_back(c_pid);
        return c_pid;
    }
    else {
        multiproc_task task(m_ourself, peer);

        /* sandbox after multiproc_task constructor
         * due to shmget, and the alike */
        task.sandbox();

        task.work();
        task.quit();

        /* keep explicit _exit() to
         * surpress compiler warnings */
        _exit(0);
    }
}

void torr::multiproc::respawner()
{
    for (auto it = m_tasks.begin(); it != m_tasks.end();) {
        const auto& pid = *it;
        int pid_status = waitpid(pid, 0, WNOHANG);
        if (pid_status < 0) {
            it = m_tasks.erase(it);
            std::println("dead {} ", pid);
            spawn();
            break;
        }

        ++it;
    }
}

void torr::multiproc::spawner()
{
    m_is_spawning = true;
    for (int i = 0; i < m_spawn_children_count; i++)
        pid_t child_pid = spawn();
    m_is_spawning = false;
}

void torr::multiproc::start()
{
    m_ourself.construct_handshake_string();
    m_main_channel.resize_capacity(65536);

    std::thread(&multiproc::spawner, this).detach();

    while (true) {
        multiproc_message message;
        int length = m_main_channel.read(sizeof(message), 1000);
        if (length < sizeof(message)) {
            if (!m_is_spawning)
                respawner();
            continue;
        }

        memcpy(&message, m_main_channel.read_data().data(), sizeof(message));

        switch (message.type) {
        case multiproc_message_type::download_piece_done:
            sem_wait(m_main_channel_mutex);
            handle_downloaded_piece(message);
            sem_post(m_main_channel_mutex);
            break;

        case multiproc_message_type::unkown:
        default:
            break;
        }
    }
}

void torr::multiproc::handle_downloaded_piece(const multiproc_message& message)
{
    m_main_channel.resize_capacity(message.payload_size);
    std::ofstream out_file(std::format("./.pieces/piece_{}.txt", message.field0), std::ios::binary);
    int receive_left = message.payload_size;

    while (receive_left > 0) {
        size_t read_size = m_main_channel.read();
        out_file.write((char*)m_main_channel.read_data().data(), read_size);
        receive_left -= read_size;
    }

    out_file.close();
    m_ourself.piece_download_complete(message.field0);
}

void torr::multiproc::set_children_count(uint8_t count)
{
    m_spawn_children_count = count;
}
