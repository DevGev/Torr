#include <ipc/ipc.hpp>
#include <cassert>
#include <sys/ipc.h>
#include <sys/shm.h>

#define IPC_FIFO_PREFIX "/tmp/torr_ipc_channel."
#define IPC_SHARED_MEMORY_PREFIX "torr__shared__"
#define IPC_TIMEOUT_USLEEP 1000000
#define IPC_TIMEOUT_TRIES 100

ipc_channel::ipc_channel()
{
    m_associated_pid = getpid();
    m_read_data.resize(1024);
}

ipc_channel::~ipc_channel()
{
    if (m_is_creator)
        unlink(m_fifo_name.c_str());
    close(m_fifo_file_descriptor);
}

void ipc_channel::set_pid(const pid_t& pid)
{
    m_associated_pid = pid;
}

const std::vector<std::byte>& ipc_channel::read_data()
{
    return m_read_data;
}

const pid_t& ipc_channel::pid()
{
    return m_associated_pid;
}

size_t ipc_channel::read(int timeout)
{
    m_polls[0].fd = m_fifo_file_descriptor;
    m_polls[0].events = POLLIN;
    int poll_value = poll(m_polls, 1, timeout);
    if (poll_value <= 0) return 0;
    
    m_read_data.clear();
    size_t size = ::read(
        m_fifo_file_descriptor,
        m_read_data.data(),
        m_read_data.capacity()
    );
    return size;
}

int ipc_channel::write(const std::span<std::byte>& data)
{
    return ::write(
        m_fifo_file_descriptor,
        (std::byte*)data.data(),
        data.size()
    );
}

std::expected<int, const char*> ipc_channel::create_channel()
{
    m_fifo_name = IPC_FIFO_PREFIX + std::to_string(m_associated_pid);
    if (mkfifo(m_fifo_name.c_str(), 0666) < 0)
        return std::unexpected("ipc channel: failed to create fifo");
    m_is_creator = true;
    m_fifo_file_descriptor = open(m_fifo_name.c_str(), O_RDWR);
    if (m_fifo_file_descriptor < 0)
        return std::unexpected("ipc channel: failed to open fifo");
    return m_fifo_file_descriptor;
}

std::expected<int, const char*> ipc_channel::connect_channel()
{
    m_fifo_name = IPC_FIFO_PREFIX + std::to_string(m_associated_pid);
    size_t timeout = IPC_TIMEOUT_TRIES;
    while (m_fifo_file_descriptor < 0 && timeout) {
        m_fifo_file_descriptor = open(m_fifo_name.c_str(), O_RDWR);
        timeout--;
        usleep(IPC_TIMEOUT_USLEEP);
    }

    if (m_fifo_file_descriptor < 0)
        return std::unexpected("ipc channel: failed to open fifo");
    return m_fifo_file_descriptor;
}

ipc_shared_memory::ipc_shared_memory(const std::string& identifier, size_t max_size)
{
    m_capacity = max_size;
    m_identifier = IPC_SHARED_MEMORY_PREFIX + identifier; 
    key_t key = ftok(identifier.c_str(), 65);
    int shmid = shmget(key, max_size, 0666 | IPC_CREAT);
    m_memory_pointer = (uint8_t*)shmat(shmid, (void*)0, 0);
    memset(m_memory_pointer, 0, max_size);
}

ipc_shared_memory::~ipc_shared_memory()
{
    if (m_memory_pointer)
        shmdt(m_memory_pointer);
}

uint8_t* ipc_shared_memory::mutable_memory_pointer()
{
    assert(m_memory_pointer);
    return m_memory_pointer;
}

const uint8_t* ipc_shared_memory::memory_pointer()
{
    assert(m_memory_pointer);
    return m_memory_pointer;
}

size_t ipc_shared_memory::capacity()
{
    return m_capacity;
}
