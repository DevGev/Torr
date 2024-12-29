#pragma once

#include <string>
#include <span>
#include <expected>
#include <vector>
#include <cstdint>
#include <poll.h>

class ipc_channel {
private:
    pid_t m_associated_pid;
    std::string m_fifo_name;
    bool m_is_creator = { 0 };
    int m_fifo_file_descriptor { -1 };
    std::vector<std::byte> m_read_data;
    struct pollfd m_polls[1];

public:
    ipc_channel();
    ~ipc_channel();

    const pid_t& pid();
    const std::vector<std::byte>& read_data();

    void set_pid(const pid_t& pid);
    size_t read(size_t max_size = 0, int timeout = -1);
    int write(const std::span<std::byte>& data);

    std::expected<int, const char*> create_channel();
    std::expected<int, const char*> connect_channel();
};

class ipc_shared_memory {
private:
    std::string m_identifier;
    size_t m_capacity;
    uint8_t* m_memory_pointer {};

public:
    uint8_t* mutable_memory_pointer();
    const uint8_t* memory_pointer();
    size_t capacity();

    ipc_shared_memory(const std::string& identifier, size_t max_size);
    ~ipc_shared_memory();
};
