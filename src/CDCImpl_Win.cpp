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

#include <windows.h>
#include <CDCImpl.h>
#include <CDCMessageParser.h>
#include <CDCImplPri.h>
#include <string>
#include <sstream>
#include <iostream>

using namespace std;

/*
* Converts specified character string to wide-character string and returns it.
*/
wchar_t* convertToWideChars(const char* charStr);

/*
* Prints configuration setting of port.
* For testing purposes only.
*/
void printCommState(DCB dcb);

/*
* Prints specified timeouts on standard output.
* For testing purposes only.
*/
void printTimeouts(COMMTIMEOUTS timeouts);

/*
* Appends COM-port prefix to the specified COM-port and returns it.
*/
LPTSTR getCompletePortName(LPCTSTR portName);

/////////////////////////////////////////////////
/*
 *	Function of reading thread of incoming COM port messages.
 */
int CDCImplPrivate::readMsgThread()
{
    DWORD eventFlags = EV_RXCHAR;
    ustring receivedBytes;
    receivedBytes.clear();

    DWORD bytesTotal = 0;
    unsigned char byteRead = '\0';

    BOOL fWaitingOnRead = FALSE;
    DWORD occurredEvent = 0;

    int count1 = 0;
    int count2 = 0;

    OVERLAPPED overlap;
    //SecureZeroMemory(&overlap, sizeof(OVERLAPPED));
    memset(&overlap, 0, sizeof(OVERLAPPED));

    try {
        // critical initialization setting - if it fails, cannot continue
        if (!SetCommMask(portHandle, eventFlags))
            THROW_EXCEPT(CDCReceiveException, "SetCommMask failed with error " << GetLastError());

        // critical initialization setting - if it fails, cannot continue
        overlap.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (overlap.hEvent == NULL)
            THROW_EXCEPT(CDCReceiveException, "Create read char event failed with error " << GetLastError());

        HANDLE waitEvents[2];
        waitEvents[0] = overlap.hEvent;
        waitEvents[1] = readEndEvent;

        // signal for main thread to start incoming user requests
        setMyEvent(readStartEvent);

        //READ_BEGIN:
        bool run = true;
        while (run) {
            //cout << "WaitCommEvent" << endl;
            DWORD waitEventResult = WaitCommEvent(portHandle, &occurredEvent, &overlap);

            if (!waitEventResult) {
                if (GetLastError() != ERROR_IO_PENDING)  {
                    THROW_EXCEPT(CDCReceiveException, "Waiting for char event failed with error " << GetLastError());
                } else {
                    // Waiting for WaitCommEvent to finish
                    //cout << "WaitCommEvent waiting to finish" << endl;

                    // Wait a little while for an event to occur.
                    //const DWORD READ_TIMEOUT = 500;
                    DWORD waitResult = WaitForMultipleObjects(2, waitEvents, FALSE, INFINITE);
                    switch (waitResult) {
                    // Event occurred.
                    case WAIT_OBJECT_0:
                        if (!GetOverlappedResult(portHandle, &overlap, &bytesTotal, FALSE)) {
                            THROW_EXCEPT(CDCReceiveException, "Waiting for char event failed with error " << GetLastError());
                        } else {
                            // WaitCommEvent returned.
                            // Deal with reading event as appropriate.
                            //cout << "WaitCommEvent returned in overlapped" << endl;
                        }
                        break;

                    case (WAIT_OBJECT_0 + 1):
                        //cout << "End reading thread..." << endl;
                        run = false;

                    case WAIT_TIMEOUT:
                        // Operation isn't complete yet. fWaitingOnStatusHandle flag
                        // isn't changed since I'll loop back around and I don't want
                        // to issue another WaitCommEvent until the first one finishes.
                        //
                        // This is a good time to do some background work.
                        break;

                    default:
                        // Error in the WaitForSingleObject; abort
                        // This indicates a problem with the OVERLAPPED structure's
                        // event handle.
                        break;
                    }
                }
            } else {
                // WaitCommEvent returned immediately.
                // Deal with reading event as appropriate.
                //cout << "WaitCommEvent returned immediately" << endl;
            }

            //reading loop
            do {
                if (!fWaitingOnRead) {

                    // Issue read operation - overlapped - just once, must waiting for completion
                    DWORD dwReadResult = ReadFile(portHandle, &byteRead, 1, &bytesTotal, &overlap);

                    if (!dwReadResult) {
                        // read not delayed?
                        if (GetLastError() != ERROR_IO_PENDING) {
                            // Error in communications; report it.
                            THROW_EXCEPT(CDCReceiveException, "Reading failed with error " << GetLastError());
                        } else {
                            //cout << "Waiting for reading..." << endl;

                            //cout << "Read result:" << dwReadResult << endl;
                            //cout << "Last error:" << GetLastError() << endl;
                            //cout << "Read byte:" << byteRead << endl;
                            //cout << "TotalBytes:" << bytesTotal << endl;

                            fWaitingOnRead = TRUE;
                        }
                    } else {
                        // read completed immediately
                        //cout << "Reading immediately:" << ++count1 << endl;

                        //cout << "Read result:" << dwReadResult << endl;
                        //cout << "Last error:" << GetLastError() << endl;
                        //cout << "Read byte:" << byteRead << endl;
                        //cout << "TotalBytes:" << bytesTotal << endl;

                        receivedBytes.push_back(byteRead);

                        if (byteRead == 0x0D) {
                            //basic_string<unsigned char>::iterator its;
                            //cout.setf(ios_base::hex, ios_base::basefield);
                            //cout.setf(ios_base::uppercase);
                            //for(its = receivedBytes.begin(); its != receivedBytes.end(); its++)
                            //	cout << setw(3) << (int) *its;
                            //cout << endl;
                            processAllMessages(receivedBytes);
                        }

                        // ready to read out another byte (bytesTotal) if available
                        fWaitingOnRead = FALSE;
                    }
                }

                if (fWaitingOnRead) {
                    //cout << "WaitForMultipleObjects" << endl;

                    //const DWORD READ_TIMEOUT = 500;
                    DWORD waitResult = WaitForMultipleObjects(2, waitEvents, FALSE, INFINITE);
                    switch (waitResult) {
                    // Read completed.
                    case WAIT_OBJECT_0:
                        if (!GetOverlappedResult(portHandle, &overlap, &bytesTotal, FALSE)) {
                            THROW_EXCEPT(CDCReceiveException, "Waiting for reading event failed with error " << GetLastError());
                        } else {
                            // Read completed successfully.
                            //cout << "Reading overlap:" << ++count2 << endl;
                            //cout << "TotalBytes:" << bytesTotal << endl;

                            if (bytesTotal != 0) {
                                //cout << "Read byte:" << byteRead << endl;
                                receivedBytes.push_back(byteRead);
                            }

                            if (byteRead == 0x0D) {
                                //basic_string<unsigned char>::iterator its;
                                //cout.setf(ios_base::hex, ios_base::basefield);
                                //cout.setf(ios_base::uppercase);
                                //for(its = receivedBytes.begin(); its != receivedBytes.end(); its++)
                                //	cout << setw(3) << (int) *its;
                                //cout << endl;
                                processAllMessages(receivedBytes);
                            }
                        }
                        //  Reset flag so that another opertion can be issued.
                        fWaitingOnRead = FALSE;
                        break;

                    case (WAIT_OBJECT_0 + 1):
                        //cout << "End reading thread..." << endl;
                        run = false;

                    case WAIT_TIMEOUT:
                        // Operation isn't complete yet. fWaitingOnRead flag isn't
                        // changed since I'll loop back around, and I don't want
                        // to issue another read until the first one finishes.
                        //
                        // This is a good time to do some background work.
                        //cout << "Reading timeouted..." << endl;
                        break;

                    default:
                        // Error in the WaitForSingleObject; abort.
                        // This indicates a problem with the OVERLAPPED structure's
                        // event handle.
                        THROW_EXCEPT(CDCReceiveException, "Waiting for event in read cycle failed with error " << GetLastError());
                    }
                }
            } while (bytesTotal);
        }
    }
    //READ_ERROR:
    catch (CDCReceiveException &e) {
        CloseHandle(overlap.hEvent);
        setLastReceptionError(e.what());
        setReceptionStopped(true);
        return 1;
    }
    catch (CDCImplException &e) {
        CloseHandle(overlap.hEvent);
        setLastReceptionError(e.what());
        setReceptionStopped(true);
        return 1;
    }

    //READ_END:
    CloseHandle(overlap.hEvent);
    return 0;
}

/*
 * Sends command stored in buffer to COM port.
 * @param cmd command to send to COM-port.
 */
void CDCImplPrivate::sendCommand(Command& cmd)
{
    resetMyEvent(newMsgEvent);

    OVERLAPPED overlap;
    //SecureZeroMemory(&overlap, sizeof(OVERLAPPED));
    memset(&overlap, 0, sizeof(OVERLAPPED));

    overlap.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (overlap.hEvent == NULL)
        THROW_EXCEPT(CDCSendException, "Creating send event failed with error " << GetLastError());

    BuffCommand buffCmd = commandToBuffer(cmd);
    DWORD bytesWritten = 0;
    if (!WriteFile(portHandle, buffCmd.cmd, buffCmd.len, &bytesWritten, &overlap)) {
        if (GetLastError() != ERROR_IO_PENDING) {
            THROW_EXCEPT(CDCSendException, "Sending message failed with error " << GetLastError());
        } else {
            DWORD waitResult = WaitForSingleObject(overlap.hEvent, TM_SEND_MSG);
            switch (waitResult) {
            case WAIT_OBJECT_0:
                if (!GetOverlappedResult(portHandle, &overlap, &bytesWritten, FALSE)) {
                    THROW_EXCEPT(CDCSendException, "Waiting for send failed with error " << GetLastError());
                } else {
                    // Write operation completed successfully
                }
                break;

            case WAIT_TIMEOUT:
                THROW_EXCEPT(CDCSendException, "Waiting for send timeouted");

            default:
                THROW_EXCEPT(CDCSendException, "Waiting for send failed with error " << GetLastError());
            }
        }
    } else {
        // Write operation completed successfully
    }

    CloseHandle(overlap.hEvent);
}

///////////////////////////////////////////
void CDCImplPrivate::setMyEvent(HANDLE evnt)
{
    if (!SetEvent(evnt))
        THROW_EXCEPT(CDCImplException, "Signaling anew message event failed with error " << GetLastError());
}

void CDCImplPrivate::resetMyEvent(HANDLE evnt)
{
    if (!ResetEvent(evnt))
        THROW_EXCEPT(CDCImplException, "Reset start read event failed with error " << GetLastError());
}

void CDCImplPrivate::createMyEvent(HANDLE & evnt)
{
    evnt = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (evnt == NULL)
        THROW_EXCEPT(CDCImplException, "Create anew message event failed with error " << GetLastError());
}

void CDCImplPrivate::destroyMyEvent(HANDLE & evnt)
{
    CloseHandle(evnt);
}

/*
* Waits for response of sent message.
* @return WAIT_OBJECT_0 - response OK.
*		    WAIT_TIMEOUT - waiting timeouted
*			other value - error
*/
DWORD CDCImplPrivate::waitForMyEvent(HANDLE evnt, DWORD timeout)
{
    std::stringstream excStream;
    DWORD waitResult = WaitForSingleObject(evnt, timeout);
    switch (waitResult) {
    case WAIT_OBJECT_0:
        // OK
        break;
    case WAIT_TIMEOUT:
        THROW_EXCEPT(CDCReceiveException, "Waiting for event timeouted");
    default:
        THROW_EXCEPT(CDCReceiveException, "WaitForSingleObject failed with error " << GetLastError());
    }

    return waitResult;
}

/* Configures and opens port for communication. */
HANDLE CDCImplPrivate::openPort(const std::string& portName)
{
    std::string portNameU(portName);
    if (portNameU.empty())
        portNameU = "COM1";
#ifdef _UNICODE
    LPCWSTR wcharCommPort = convertToWideChars(portNameU);
    if (wcharCommPort == L'\0')
        THROW_EX(CDCImplException, "Port name character conversion failed");
    portNameU = wcharCommPort;
    delete[] wcharCommPort;
#else
    portNameU = portName;
#endif

    LPTSTR completePortName = getCompletePortName(portNameU.c_str());
    if (completePortName == NULL)
        THROW_EXCEPT(CDCImplException, "Complete port name creation failed");

    HANDLE portHandle = CreateFile(completePortName,
        GENERIC_READ | GENERIC_WRITE, // read and write
        0,      //  must be opened with exclusive-access
        NULL,   //  default security attributes
        OPEN_EXISTING, //  must use OPEN_EXISTING
        FILE_FLAG_OVERLAPPED, // overlapped operation
        NULL); //   must be NULL for comm devices

    //  Handle the error.
    if (portHandle == INVALID_HANDLE_VALUE)
        THROW_EXCEPT(CDCImplException, "Port handle creation failed with error " << GetLastError());

    delete[] completePortName;

    DCB dcb;
    //SecureZeroMemory(&dcb, sizeof(DCB));
    memset(&dcb, 0, sizeof(DCB));
    dcb.DCBlength = sizeof(DCB);

    BOOL getStateResult = GetCommState(portHandle, &dcb);
    if (!getStateResult)
        THROW_EXCEPT(CDCImplException, "Port state getting failed with error " << GetLastError());

    // set comm parameters
    dcb.BaudRate = CBR_57600;     //  baud rate
    dcb.ByteSize = 8;             //  data size, xmit and rcv
    dcb.Parity = NOPARITY;      //  parity bit
    dcb.StopBits = ONESTOPBIT;    //  stop bit

    BOOL setStateResult = SetCommState(portHandle, &dcb);
    if (!setStateResult)
        THROW_EXCEPT(CDCImplException, "Port state setting failed with error " << GetLastError());

    // printCommState(dcb);

    COMMTIMEOUTS timeouts;
    //SecureZeroMemory(&timeouts, sizeof(COMMTIMEOUTS));
    memset(&timeouts, 0, sizeof(COMMTIMEOUTS));

    BOOL getToutsResult = GetCommTimeouts(portHandle, &timeouts);
    if (!getToutsResult)
        THROW_EXCEPT(CDCImplException, "Port timeouts getting failed with error " << GetLastError());

    //printTimeouts(timeouts);

    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(portHandle, &timeouts))
        THROW_EXCEPT(CDCImplException, "Port timeouts setting failed with error " << GetLastError());

    return portHandle;
}

void CDCImplPrivate::closePort(HANDLE & portHandle)
{
  CloseHandle(portHandle);
}

/////////////////////////////////////
/*
* Converts specified character string to wide-character string and returns it.
*/
wchar_t* convertToWideChars(const char* charStr)
{
    size_t charsToConvert = mbstowcs(NULL, charStr, strlen(charStr));
    size_t maxCount = charsToConvert + 1;
    wchar_t* wcStr = ant_new wchar_t[charsToConvert + 1];
    memcpy(wcStr, '\0', (charsToConvert + 1) * sizeof(wchar_t));
    size_t convertedChars = mbstowcs(wcStr, charStr, maxCount);
    /*
    if (mbstowcs_s(&convertedChars, wcStr, charStrSize, charStr, _TRUNCATE) != 0) {
    return L'\0';
    }
    */
    if (charsToConvert != convertedChars)
        return L'\0';

    return wcStr;
}

/*
* Prints configuration setting of port.
* For testing purposes only.
*/
void printCommState(DCB dcb)
{
    printf("\nBaudRate = %d, ByteSize = %d, Parity = %d, StopBits = %d\n",
        dcb.BaudRate,
        dcb.ByteSize,
        dcb.Parity,
        dcb.StopBits);
}

/*
* Prints specified timeouts on standard output.
* For testing purposes only.
*/
void printTimeouts(COMMTIMEOUTS timeouts)
{
    printf("\nRead interval = %d, Read constant = %d, "
        "Read multiplier = %d, Write constant = %d, Write multiplier = %d\n",
        timeouts.ReadIntervalTimeout,
        timeouts.ReadTotalTimeoutConstant,
        timeouts.ReadTotalTimeoutMultiplier,
        timeouts.WriteTotalTimeoutConstant,
        timeouts.WriteTotalTimeoutMultiplier);
}

/*
* Appends COM-port prefix to the specified COM-port and returns it.
*/
LPTSTR getCompletePortName(LPCTSTR portName)
{
    LPTSTR portPrefix = "\\\\.\\";

    int completeSize = lstrlen(portPrefix) + lstrlen(portName);
    LPTSTR completePortName = ant_new TCHAR[completeSize + 1];
    memset(completePortName, '\0', (completeSize + 1) * sizeof(TCHAR));
    if (lstrcat(completePortName, portPrefix) == NULL)
        return NULL;

    if (lstrcat(completePortName, portName) == NULL)
        return NULL;

    return completePortName;
}
