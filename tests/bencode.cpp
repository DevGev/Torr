#include <ak/bencode_map.hpp>
#include <print>

#define TEST_NAME "ak/bencode_map.hpp"
#define TEST_BENCODE_STRING "d4:wiki7:bencode7:meaningi42e4:hitsli32ei22ed5:C++204:cooleeee"

int test_bencode_main()
{
    std::print("test: {} ... ", TEST_NAME);

    bencode_map bencode;
    bencode.from_string(TEST_BENCODE_STRING);

    bencode["wiki"].as_str();
    bencode["meaning"].as_int();
    bencode["hits"][0].as_int();
    bencode["hits"][1].as_int();
    bencode["hits"][2]["C++20"].as_str();

    std::println("passed");

    return 0;
}
