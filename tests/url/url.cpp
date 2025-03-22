#include <generic/try.hpp>
#include <uri/url.hpp>
#include <cassert>
#include <print>

#define TEST_NAME "uri/url.cpp"

#define TEST_URL_STRING "https://subdomain.domain.com/a/a/d/f/abc%4112=1231%0501-.3=1" 
#define EXPECTED_PROTOCOL torr::url::protocol_type::https
#define EXPECTED_HOST "subdomain.domain.com" 
#define EXPECTED_PORT 443
#define EXPECTED_PATH "/a/a/d/f/"
#define EXPECTED_QUERY "abc%4112=1231%0501-.3=1"

#define TEST_URL_STRING2 "https://a.a.com:11443501"
#define EXPECTED_PORT2 11443501

int main()
{
    std::print("test: {} ... ", TEST_NAME);

    auto url = torr::url();
    MUST(url.from_string(std::string(TEST_URL_STRING)));

    assert(
        url.protocol() == EXPECTED_PROTOCOL &&
        "failed due to url.protocol()"
    );

    assert(
        url.host() == EXPECTED_HOST &&
        "failed due to url.host()"
    );

    assert(
        url.port() == EXPECTED_PORT &&
        "failed due to url.port()"
    );

    assert(
        url.path() == EXPECTED_PATH &&
        "failed due to url.path()"
    );

    assert(
        url.query() == EXPECTED_QUERY &&
        "failed due to url.query()"
    );

    assert(
        url.from_string(std::string(TEST_URL_STRING2)).value().port() == EXPECTED_PORT2 &&
        "failed due to url.port()"
    );

    std::println("passed");

    return 0;
}
