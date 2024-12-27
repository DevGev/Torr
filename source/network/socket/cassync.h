#ifndef CASSYNC_H
#define CASSYNC_H

#include <sys/socket.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <errno.h>

static int connect_with_timeout(int sockfd, const struct sockaddr *addr, socklen_t addrlen, unsigned int timeout_ms)
{
    int rc = 0;
    int sockfd_flags_before;
    if ((sockfd_flags_before = fcntl(sockfd, F_GETFL, 0) < 0)) return -1;
    if (fcntl(sockfd, F_SETFL, sockfd_flags_before | O_NONBLOCK) < 0) return -1;

    do {
      if (connect(sockfd, addr, addrlen) < 0) {
        if ((errno != EWOULDBLOCK) && (errno != EINPROGRESS)) {
          rc = -1;
        }
        else {
          struct timespec now;
          if (clock_gettime(CLOCK_MONOTONIC, & now) < 0) {
            rc = -1;
            break;
          }
          struct timespec deadline = {
            .tv_sec = now.tv_sec,
            .tv_nsec = now.tv_nsec + timeout_ms * 1000000l
          };
          do {
            if (clock_gettime(CLOCK_MONOTONIC, & now) < 0) {
              rc = -1;
              break;
            }
            int ms_until_deadline = (int)((deadline.tv_sec - now.tv_sec) * 1000l +
              (deadline.tv_nsec - now.tv_nsec) / 1000000l);
            if (ms_until_deadline < 0) {
              rc = 0;
              break;
            }
            struct pollfd pfds[] = {
              {
                .fd = sockfd, .events = POLLOUT
              }
            };
            rc = poll(pfds, 1, ms_until_deadline);
            if (rc > 0) {
              int error = 0;
              socklen_t len = sizeof(error);
              int retval = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, & error, & len);
              if (retval == 0) errno = error;
              if (error != 0) rc = -1;
            }
          }

          while (rc == -1 && errno == EINTR);
          if (rc == 0) {
            errno = ETIMEDOUT;
            rc = -1;
          }
        }
      }
    } while (0);
    if (fcntl(sockfd, F_SETFL, sockfd_flags_before) < 0) return -1;
    return rc;
}

#endif
