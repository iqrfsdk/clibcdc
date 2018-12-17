/*
 * Copyright 2018 IQRF Tech s.r.o.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/eventfd.h>
#include <termios.h>
#include <fcntl.h>

#include <sys/time.h>
#include <unistd.h>
#include <sys/select.h>

#include <iostream>
#include <errno.h>
#include <limits.h>

#include <cdc/CDCImpl.h>
#include <cdc/CDCImplPri.h>
#include <cdc/CDCPlatforSpec_Lin.h>

using namespace std;

/* Wrapper for standard 'select' function. */
int selectEvents(std::set<int>& fds, EventType evType, unsigned int timeout)
{
    if (fds.empty())
        return 0;

    int maxFd = 0;
    fd_set selFds;
    FD_ZERO(&selFds);

    set<int>::iterator fdsIt = fds.begin();
    for (;fdsIt != fds.end(); fdsIt++) {
        FD_SET(*fdsIt, &selFds);
        if (*fdsIt > maxFd)
            maxFd = (*fdsIt);
    }

    maxFd++;
    if (timeout != 0) {
        struct timeval waitTime;
        waitTime.tv_sec = timeout;
        waitTime.tv_usec = 0;

        if (evType == READ_EVENT)
            return select(maxFd, &selFds, NULL, NULL, &waitTime);

        if (evType == WRITE_EVENT)
            return select(maxFd, NULL, &selFds, NULL, &waitTime);

        // no other event type - params error
        return -1;
    }

    if (evType == READ_EVENT)
        return select(maxFd, &selFds, NULL, NULL, NULL);

    if (evType == WRITE_EVENT)
        return select(maxFd, NULL, &selFds, NULL, NULL);

    // no other event type - params error
    return -1;
}
