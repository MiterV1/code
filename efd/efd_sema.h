#ifndef __EFD_SEMA_H__
#define __EFD_SEMA_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/eventfd.h>

static inline int semaphore_new(void) {
    return eventfd(0ULL, EFD_SEMAPHORE);
}

static inline bool semaphore_set_nonblock(int fd) {
    int flags, ret;

    flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        printf("%s unable to get flags for semaphore fd: %s", __func__,
                strerror(errno));
        return false;
    }

    ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (ret == -1) {
        printf("%s unable to set O_NONBLOCK for semaphore fd: %s", __func__,
                strerror(errno));
        return false;
    }

    return true;
}

static inline bool semaphore_try_wait(int fd) {
    eventfd_t value;
    if (eventfd_read(fd, &value) == -1) {
        return false;
    }

    return true;
}

static inline void semaphore_wait(int fd) {
    int64_t value;
    if (eventfd_read(fd, &value) == -1) {
        printf("%s unable to wait on semaphore: %s", __func__, strerror(errno));
    }
}

static inline void semaphore_post(int fd) {
    if (eventfd_write(fd, 1ULL) == -1) {
        printf("%s unable to post to semaphore: %s", __func__, strerror(errno));
    }
}

#endif