extern int test_url_main();
extern int test_magnet_main();
extern int test_network_main();
extern int test_bencode_main();
extern int test_torrent_file_main();

int main()
{
    test_bencode_main();
    test_torrent_file_main();
    test_magnet_main();
    test_url_main();
    test_network_main();
    return 0;
}
