#include <multiproc/sandbox.h>
#include <print>
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/utsname.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <cassert>

#define TEST_NAME "multiproc/sandbox.c"

int main()
{
    std::print("test: {} ... ", TEST_NAME);
    unlink("sandbox_test_file");

    pid_t pid = fork();
    assert(pid >= 0 && "failed due to fork()");

    if (pid == 0) {
        assert(
            sandbox_landlock_process() &&
            "failed due to sandbox_landlock_process()"
        );
        assert(
            sandbox_seccomp_filter_process() &&
            "failed due to sandbox_seccomp_filter_process"
        );

        struct utsname u;
        uname(&u);

        int fd = open("sandbox_test_file", O_RDWR | O_CREAT);
        write(fd, "hi", 2);
        close(fd);
    }

    else {
        usleep(1000000);
        struct stat sts;

        assert(
            stat("sandbox_test_file", &sts) < 0 &&
            "failed due to sandbox not blocking syscalls"
        );
    }

    std::println("passed");
    return 0;
}
