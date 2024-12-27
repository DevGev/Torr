#pragma once

#include <arpa/inet.h>
#include <stdint.h>

#define htonll(x) ((1==htonl(1)) ? (x) : ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#define ntohll(x) ((1==ntohl(1)) ? (x) : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))

class big_endian_uint16_t {
public:
        big_endian_uint16_t() : be_val_(0) { }
        big_endian_uint16_t(const uint16_t &val) : be_val_(htons(val)) { }
        uint16_t as_small_endian() const { return ntohs(be_val_); }
        operator uint16_t() const { return ntohs(be_val_); }
private:
        uint16_t be_val_;
} __attribute__((packed));

class big_endian_uint32_t {
public:
        big_endian_uint32_t() : be_val_(0) { }
        big_endian_uint32_t(const uint32_t &val) : be_val_(htonl(val)) { }
        uint32_t as_small_endian() const { return ntohl(be_val_); }
        operator uint32_t() const { return ntohl(be_val_); }
private:
        uint32_t be_val_;
} __attribute__((packed));

class big_endian_uint64_t {
public:
        big_endian_uint64_t() : be_val_(0) { }
        big_endian_uint64_t(const uint64_t &val) : be_val_(htonll(val)) { }
        uint64_t as_small_endian() const { return ntohll(be_val_); }
        operator uint64_t() const { return ntohll(be_val_); }
private:
        uint64_t be_val_;
} __attribute__((packed));
