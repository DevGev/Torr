#include "sandbox.h"
#include <seccomp.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <cstring>
#include <cassert>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <linux/limits.h>
#include <linux/landlock.h>
#include <linux/prctl.h>

/* set macro to 1 to trap seccomp intercepted syscalls
 * signal handler output on crash should be:
 * si_signo: (should be SIGSYS)
 * si_value: (syscall # for unpermitted syscall) */
#define DEBUG_SANDBOX 0

/* sandbox all filesystem actions except for fifos */
static const struct landlock_ruleset_attr landlock_rules_blacklist = {
.handled_access_fs =
    LANDLOCK_ACCESS_FS_EXECUTE |
    LANDLOCK_ACCESS_FS_WRITE_FILE |
    LANDLOCK_ACCESS_FS_READ_FILE |
    LANDLOCK_ACCESS_FS_READ_DIR |
    LANDLOCK_ACCESS_FS_REMOVE_DIR |
    LANDLOCK_ACCESS_FS_REMOVE_FILE |
    LANDLOCK_ACCESS_FS_MAKE_CHAR |
    LANDLOCK_ACCESS_FS_MAKE_DIR |
    LANDLOCK_ACCESS_FS_MAKE_REG |
    LANDLOCK_ACCESS_FS_MAKE_SYM |
    LANDLOCK_ACCESS_FS_MAKE_BLOCK |
    LANDLOCK_ACCESS_FS_TRUNCATE |
    LANDLOCK_ACCESS_FS_REFER,
};

static const int seccomp_filter_whitelist[] {
    /* necessary for exit operations */
    SCMP_SYS(exit_group),
    SCMP_SYS(exit),

    /* necessary for IO ex. printf */
    /* read() write() close() file operations unpermitted by landlock,
     * can therefore be kept allowed by seccomp */
    SCMP_SYS(read),
    SCMP_SYS(write),
    SCMP_SYS(close),

    /* necessary for torrent peer TCP connection */
    SCMP_SYS(socket),
    SCMP_SYS(connect),
    SCMP_SYS(recvfrom),
    SCMP_SYS(sendto),
    SCMP_SYS(sched_yield),

    /* necessary for heap allocations
     * NOTE: mseal() memory to harden security? */
    SCMP_SYS(msync),
    SCMP_SYS(mmap),
    SCMP_SYS(munmap),
    SCMP_SYS(brk)
};

#if DEBUG_SANDBOX
static const uint32_t seccomp_action = SECCOMP_RET_TRAP;
#else
static const uint32_t seccomp_action = SECCOMP_RET_KILL;
#endif

#ifndef landlock_create_ruleset
static inline int landlock_create_ruleset(const struct landlock_ruleset_attr* const attr,
	const size_t size, const __u32 flags)
{
	return syscall(__NR_landlock_create_ruleset, attr, size, flags);
}
#endif

#ifndef landlock_restrict_self
static inline int landlock_restrict_self(const int ruleset_fd,
	const __u32 flags)
{
	return syscall(__NR_landlock_restrict_self, ruleset_fd, flags);
}
#endif

bool sandbox_landlock_process()
{
	int ruleset_fd = landlock_create_ruleset(&landlock_rules_blacklist,
        sizeof(landlock_rules_blacklist), 0);
	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0))
        return false;
	return (!landlock_restrict_self(ruleset_fd, 0));
}

bool sandbox_seccomp_filter_process()
{
#if DEBUG_SANDBOX
    printf("warning: debugging sandbox, entering unsafe environment");
    if (!sandbox_install_sigsys_handler())
        return false;
#endif

    scmp_filter_ctx ctx = seccomp_init(seccomp_action);

    const size_t whitelist_size = sizeof(seccomp_filter_whitelist) / sizeof(seccomp_filter_whitelist[0]);
    for (int i = 0; i < whitelist_size; ++i) {
        if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, seccomp_filter_whitelist[i], 0) < 0)
            return false;
    }
    if (seccomp_load(ctx) < 0)
        return false;

    seccomp_release(ctx);
    return true;
}

static void
sigsys_handler(int sig, siginfo_t* siginfo, void* ucontext)
{
    static int cnt = 0;
    (void)sig;
    (void)ucontext;
    cnt++;

    printf("#%d\n", cnt);
    printf("\tsi_signo: %d (%s)\n", siginfo->si_signo, strsignal(siginfo->si_signo));
    printf("\tsi_value.: %d\n", siginfo->si_value);
    if (siginfo->si_code == SI_USER || siginfo->si_code == SI_QUEUE) {
        printf("\tsi_pid..: %d\n", siginfo->si_pid);
        printf("\tsi_uid..: %d\n", siginfo->si_uid);
    }

    return;
}

bool sandbox_install_sigsys_handler()
{
    struct sigaction act;
    sigset_t mask;

    memset(&act, 0x00, sizeof(act));
    sigemptyset(&act.sa_mask);
    act.sa_sigaction = sigsys_handler;
    act.sa_flags = SA_SIGINFO;

    if (sigaction(SIGSYS, &act, NULL) == -1)
        return false;

    sigemptyset(&mask);
    sigaddset(&mask, SIGSYS);
    return true;
}
