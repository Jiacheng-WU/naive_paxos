//
// Created by Jiacheng Wu on 11/8/22.
//

#ifndef PAXOS_SYNC_HPP
#define PAXOS_SYNC_HPP

#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <iostream>
#include <fstream>


//! Flushes buffered data and attributes written to the file to permanent storage
inline int full_sync(std::fstream &f) {
    return f.rdbuf()->pubsync();
}

inline int full_sync(int fd) {
    while (true) {
#if defined(__APPLE__) && defined(__MACH__) && defined(F_FULLFSYNC)
        // Mac OS does not flush data to physical storage with fsync()
        int err = ::fcntl(fd, F_FULLFSYNC);
#else
        int err = ::fsync(fd);
#endif
        if (err < 0) [[unlikely]] {
            err = errno;
            // POSIX says fsync can return EINTR (https://pubs.opengroup.org/onlinepubs/9699919799/functions/fsync.html).
            // fcntl(F_FULLFSYNC) isn't documented to return EINTR, but it doesn't hurt to check.
            if (err == EINTR)
                continue;

            return err;
        }

        break;
    }

    return 0;
}

#endif //PAXOS_SYNC_HPP
