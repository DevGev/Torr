#ifndef SANDBOX_H
#define SANDBOX_H

/* seccomp sandboxing see https://man7.org/linux/man-pages/man2/seccomp.2.html
 * requires libseccomp https://github.com/seccomp/libseccomp
 * blocks unpermitted syscalls */
bool sandbox_seccomp_filter_process();

/* landlock since Linux 5.13 see https://man7.org/linux/man-pages/man7/landlock.7.html
 * NOTE: should be called before sandbox_seccomp_filter_process()
 * as seccomp will block the landlock syscall */
bool sandbox_landlock_process();

/* install a SIGSYS handler to debug process exits envoked by seccomp */
bool sandbox_install_sigsys_handler();

#endif
