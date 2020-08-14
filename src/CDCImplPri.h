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

#pragma once

#include "CDCTypes.h"

#ifdef WIN32
#include <windows.h>
#endif
#include <CDCMessageParser.h>
#include <map>
#include <thread>
#include <mutex>
#include <string>

#ifdef WIN32
//typedef void* HANDLE;
static const DWORD scond = 1000;
#else
typedef unsigned long DWORD;
typedef int HANDLE;
static const DWORD scond = 1;
typedef void* LPVOID;
#endif

/*
* Implementation class.
*/
class CDCImplPrivate {
public:
    CDCImplPrivate();
    CDCImplPrivate(const char* commPort);
    ~CDCImplPrivate();

    /* Command, which will be sent to COM-port. */
    struct Command {
        MessageType msgType;
        ustring data;
    };

    /* Bufferized command for passing to COM-port. */
    struct BuffCommand {
        unsigned char* cmd;
        DWORD len;
    };

    /* Info about parsed data. */
    struct ParsedMessage {
        ustring message;
        ParseResult parseResult;
    };


    /* OPERATION TIMEOUTS. */
    /* Starting read thread. */
    static const DWORD TM_START_READ = 5 * scond;

    /* Canceling read thread. */
    //static const DWORD TM_CANCEL_READ = 5*scond;

    /* Sending message to COM-port. */
    static const DWORD TM_SEND_MSG = 5 * scond;

    /* Waiting for a response. */
    static const DWORD TM_WAIT_RESP = 5 * scond;

    HANDLE portHandle;		// handle to COM-port
    std::string m_commPort;

    std::thread readMsgHandle;

    /*
    * Signal for main thread, that a message-respond was read from COM-port.
    */
    HANDLE newMsgEvent;

    /* Signal for main thread, that read thread has started. */
    HANDLE readStartEvent;

    /* Signal for read thread to cancel. */
    HANDLE readEndEvent;

    HANDLE readEndResponse;

    /* Mapping from message types to response headers. */
    std::map<MessageType, std::string> messageHeaders;

    /* Parser of incoming messages from COM-port. */
    CDCMessageParser* msgParser;

    /*
    * Last received parsed response from COM-port.
    * Not asynchronous message.
    */
    ParsedMessage lastResponse;

    /* Registered listener of asynchronous messages reception. */
    AsyncMsgListenerF asyncListener;
    void setAsyncListener(AsyncMsgListenerF listener);

    /* Indicates, whether is reading thread stopped. */
    bool receptionStopped;
    void setReceptionStopped(bool value);
    bool getReceptionStopped(void);

    /* Last reception error. */
    std::string lastReceptionError; //char* lastReceptionError;
    void setLastReceptionError(const std::string& descr);
    std::string cloneLastReceptionError();



    /* INITIALIZATION. */
    /* Encapsulates basic initialization process. */
    void init(void);

    /* Initializes messageHeaders map. */
    void initMessageHeaders(void);

    /* Initializes lastResponse. */
    void initLastResponse(void);

    /* Function of reading thread of incoming COM port messages. */
    int readMsgThread();

    /* Reads data from port and appends them to the specified buffer. */
    //int appendDataFromPort(LPOVERLAPPED overlap, ustring& destBuffer);

    /* Extracts and process all messages in specified buffer. */
    void processAllMessages(ustring& msgBuffer);

    /* Parses next message string from specified buffer and returns it. */
    ParsedMessage parseNextMessage(ustring& msgBuffer);

    /* Processes specified message - include parsing. */
    void processMessage(ParsedMessage& parsedMessage);


    /* COMMAND - RESPONSE CYCLE. */

    /* Construct command and returns it. */
    Command constructCommand(MessageType msgType, ustring data);

    /* Sends command, waits for response a checks the response. */
    void processCommand(Command& cmd);

    /* Sends command stored in buffer to COM port. */
    void sendCommand(Command& cmd);

    /* Bufferize specified command for passing to COM-port. */
    BuffCommand commandToBuffer(Command& cmd);

    /* Checks, if specified value is the correct value of SPIStatus. */
    bool isSPIStatusValue(ustring& statValue);

    int appendDataFromPort(unsigned char* buf, unsigned buflen, ustring& destBuffer);

    // critical section objects for thread safe access to some fields
    std::mutex csLastRecpError;
    std::mutex csReadingStopped;
    std::mutex csAsyncListener;

    //throws CDCReceiveException
    void setMyEvent(HANDLE evnt);
    void resetMyEvent(HANDLE evnt);
    void createMyEvent(HANDLE & evnt);
    void destroyMyEvent(HANDLE & evnt);
    DWORD waitForMyEvent(HANDLE evnt, DWORD timeout);

    HANDLE openPort(const std::string& portName);
    void closePort(HANDLE & portHandle);

    unsigned char* m_transmitBuffer;
    DWORD m_transmitBufferLen;

};
