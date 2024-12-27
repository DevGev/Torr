#pragma once
#include <stdlib.h>
#include <string>
#include <uri/url.hpp>
#include <netinet/in.h>

namespace torr {

struct endpoint {
    torr::url::protocol_type m_protocol {};
    in_addr m_ip_address {};
    size_t m_port {};
};

}
