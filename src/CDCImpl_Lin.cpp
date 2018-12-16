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

#include <CDCImpl.h>
#include <CDCMessageParser.h>
#include <CDCImplPri.h>

#include <set>

/* Information about what kind of event to wait for. */
enum EventType { READ_EVENT, WRITE_EVENT };

/* Wrapper for standard 'select' function. */
int selectEvents(std::set<int>& fds, EventType evType, unsigned int timeout);

/*
 *	Function of reading thread of incoming COM-port messages.
 */
int CDCImplPrivate::readMsgThread()
{
    ustring receivedBytes;
    fd_set waitEvents;
    std::string errorDescr;
    const size_t BUFF_SIZE = 1024;
    unsigned char buffer[BUFF_SIZE];


    try  {
        int maxEventNum = ((portHandle > readEndEvent)? portHandle:readEndEvent) + 1;

        // signal for main thread to continue with initialization
        setMyEvent(readStartEvent);

        bool run = true;
        receivedBytes.clear();
        while (run) {
            FD_ZERO(&waitEvents);
            FD_SET(portHandle, &waitEvents);
            FD_SET(readEndEvent, &waitEvents);

            int waitResult = select(maxEventNum, &waitEvents, NULL, NULL, NULL);
            switch (waitResult) {
            case -1:
                THROW_EXC(CDCReceiveException, "Waiting for event in read cycle failed with error " << errno);
                break;
            case 0:
                // only in the case of timeout period expires
                break;
            default:
                // read in characters into input buffer
                if (FD_ISSET(portHandle, &waitEvents)) {
                    int messageEnd = appendDataFromPort(buffer, BUFF_SIZE, receivedBytes);
                    if (messageEnd != -1)
                        processAllMessages(receivedBytes);
                }

                // read end
                if (FD_ISSET(readEndEvent, &waitEvents))
                    run = false; //goto READ_END;
            }
        }
    }
    catch (CDCReceiveException &e) {
        setLastReceptionError(e.what());
        setReceptionStopped(true);
        return 1;
    }

    return 0;
}

/*
 * Reads data from port and appends them to specified buffer until no
 * other data are in input buffer of the port.
 * @return position of message end character present in the specified buffer <br>
 *		   -1, if no message end character was appended into specified buffer
 * @throw CDCReceiveException
 */
int CDCImplPrivate::appendDataFromPort(unsigned char* buf, unsigned buflen, ustring& destBuffer)
{
    int messageEnd = -1;

    ssize_t readResult = read(portHandle, (void*)buf, buflen);
    if (readResult == -1)
        // error in communication
        THROW_EXC(CDCReceiveException, "Appending data from COM-port failed with error " << errno);

    destBuffer.append(buf, readResult);
    size_t endPos = destBuffer.find(0x0D);
    if (endPos != std::string::npos)
        messageEnd = endPos;

    return messageEnd;
}

/*
 * Sends command stored in buffer to COM port.
 * @param cmd command to send to COM-port.
 */
void CDCImplPrivate::sendCommand(Command& cmd)
{
    BuffCommand buffCmd = commandToBuffer(cmd);
    unsigned char* dataToWrite = buffCmd.cmd;
    int dataLen = buffCmd.len;

    std::set<int> fds;
    fds.insert(portHandle);

    while (dataLen > 0) {
        int selResult = selectEvents(fds, WRITE_EVENT, TM_SEND_MSG);
        if (selResult == -1)
            THROW_EXC(CDCSendException, "Sending message failed with error " << errno);

        if (selResult == 0)
            throw CDCSendException("Waiting for send timeouted");

        int writeResult = write(portHandle, dataToWrite, dataLen);
        if (writeResult == -1)
            THROW_EXC(CDCSendException, "Sending message failed with error " << errno);

        dataLen -= writeResult;
        dataToWrite += writeResult;
    }
}

/////////////////////////////////////////////////////////
void CDCImplPrivate::setMyEvent(HANDLE evnt)
{
    uint64_t readEndData = 1;
    ssize_t ret = write(evnt, &readEndData, sizeof(uint64_t));
    if (ret != sizeof(uint64_t))
        THROW_EXC(CDCImplException, "Signaling new message event failed with error " << errno);
}

void CDCImplPrivate::resetMyEvent(HANDLE evnt)
{
    //TODO empty
}

void CDCImplPrivate::createMyEvent(HANDLE & event)
{
    event = eventfd(0, 0);
    if (event == -1)
        THROW_EXC(CDCImplException, "Create new message event failed with error " << errno);
}

void CDCImplPrivate::destroyMyEvent(HANDLE & event)
{
    close(event);
}

/*
 * Blocks, until specified event is not in signaling state.
 * If timeout is not 0, waits at max for specified timeout(in seconds).
 */
DWORD CDCImplPrivate::waitForMyEvent(HANDLE evnt, DWORD timeout)
{
    std::set<int> events;
    events.insert(evnt);
    int waitResult = selectEvents(events, READ_EVENT, timeout);

    switch (waitResult) {
    case -1:
        THROW_EXC(CDCReceiveException, "Waiting in selectEvents failed with error " << errno);
        break;
    case 0:
        THROW_EXC(CDCReceiveException, "Waiting for event timeout");
        break;
    default:
        // OK
        //TODO aditional check - is it necessary here?
        uint64_t respData = 0;
        if (read(evnt, &respData, sizeof(uint_fast64_t)) == -1)
            THROW_EXC(CDCReceiveException, "Waiting for response failed with error " << errno);
        break;
    }
    return waitResult;
}


/* Configures and opens port for communication. */
HANDLE CDCImplPrivate::openPort(const std::string& portName)
{
    HANDLE portHandle = open(portName.c_str(), O_RDWR | O_NOCTTY);

    //  Handle the error.
    if (portHandle == -1)
        THROW_EXC(CDCImplException, "Port handle creation failed with error " << errno);

    if (isatty(portHandle) == 0)
        THROW_EXC(CDCImplException, "Specified file is not associated with terminal " << errno);

    struct termios portOptions;

    // get current settings of the serial port
    if (tcgetattr(portHandle, &portOptions) == -1)
        THROW_EXC(CDCImplException, "Port parameters getting failed with error " << errno);

    /*
     * Turn of:
     * - stripping input bytes to 7 bits
     * - discarding carriage return characters from input
     * - carriage return characters passed to the application as newline characters
     * - newline characters passed to the application as carriage return characters
     */
    portOptions.c_iflag &= ~(PARMRK | IGNBRK | BRKINT | ISTRIP | IGNCR
                            | ICRNL | INLCR | IXON);


    /*
     * Characters are transmitted as-is.
     */
    portOptions.c_oflag &= ~(OPOST);

    /*
     * Enable reading of incoming characters, 8 bits per byte.
     */
    portOptions.c_cflag |= CREAD;
    portOptions.c_cflag &= ~(CSIZE | PARENB | CSTOPB);
    portOptions.c_cflag |= CS8;

    // setting NONCANONICAL input processing mode
    portOptions.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
    portOptions.c_lflag |= NOFLSH;

    // speed settings
    cfsetispeed(&portOptions, B57600);
    cfsetospeed(&portOptions, B57600);

    // at least 1 character to read
    portOptions.c_cc[VMIN] = 1;
    portOptions.c_cc[VTIME] = 0;

    if (tcsetattr(portHandle, TCSANOW, &portOptions) == -1)
        THROW_EXC(CDCImplException, "Port parameters setting failed with error " << errno);

    // required to make flush to work because of Linux kernel bug
    if ( sleep(2) != 0 )
        THROW_EXC(CDCImplException, "Sleeping before flushing the port not elapsed");

    if ( tcflush(portHandle, TCIOFLUSH) != 0 )
        THROW_EXC(CDCImplException, "Port flushing failed with error" << errno);

    return portHandle;
}

void CDCImplPrivate::closePort(HANDLE & portHandle)
{
    close(portHandle);
}

/////////////////////////////////////
/* Wrapper for standard 'select' function. */
int selectEvents(std::set<int>& fds, EventType evType, unsigned int timeout)
{
    if (fds.empty())
        return 0;

    int maxFd = 0;
    fd_set selFds;
    FD_ZERO(&selFds);

    std::set<int>::iterator fdsIt = fds.begin();
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
