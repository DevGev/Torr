#include <multiproc/multiproc.hpp>
#include <unistd.h>
#include <memory.h>
#include <fstream>
#include <print>
#include <span>

torr::multiproc_task::multiproc_task(peer& ourself, const torrent_peer& them)
    : m_bitfield_pieces("bitfield_pieces", 2048),
    m_ourself(ourself),
    m_peer(them)
{
    m_main_channel.set_pid(getppid());
    m_main_channel.connect_channel();
}

torr::multiproc_task::~multiproc_task() {}

torr::multiproc::multiproc(peer& ourself, tracker& track)
    : m_bitfield_pieces("bitfield_pieces", 2048),
    m_ourself(ourself),
    m_tracker(track)
{
    m_main_channel.create_channel();

    ourself.set_shared_bitfield(
        (uint8_t*)m_bitfield_pieces.memory_pointer(),
        m_bitfield_pieces.capacity()
    );

    auto announcer_or_error = track.announce(m_ourself);
    if (!announcer_or_error.value())
        return;
    for (const auto& peer : announcer_or_error.value()->peers())
        m_addresses.push_back({ peer.ip_address(), peer.port() });
}

torr::multiproc::~multiproc() {}

void torr::multiproc_task::quit()
{
    exit(0);
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

    /* TODO: grab futex here */
    m_main_channel.write({ (std::byte*)&message, sizeof(message) });
    m_main_channel.write(output_data);
    /* futex release here */

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
        return c_pid;
    }
    else { 
        multiproc_task task(m_ourself, peer);
        task.work();
        task.quit();
    }
}

void torr::multiproc::start()
{
    m_ourself.construct_handshake_string();
    m_main_channel.resize_capacity(65536);

    for (int i = 0; i < m_spawn_children_count; i++)
        pid_t child_pid = spawn();

    while (true) {
        /* TODO: timeout poll() & spawn new children if some died */
        multiproc_message message;
        m_main_channel.read(sizeof(message));
        memcpy(&message, m_main_channel.read_data().data(), sizeof(message));

        switch (message.type) {
        case multiproc_message_type::download_piece_done:
            handle_downloaded_piece(message);
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
