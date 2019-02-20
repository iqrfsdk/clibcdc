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

#include <limits.h>
#include <CDCImpl.h>
#include <CDCImplPri.h>
#include <CDCMessageParser.h>
#include <sstream>
#include <algorithm>

using namespace std;

/*
* For converting string literals to unsigned string literals.
*/
inline const unsigned char* uchar_str(const char* s)
{
    return reinterpret_cast<const unsigned char*>(s);
}

/* --- PUBLIC INTERFACE */
CDCImpl::CDCImpl()
{
    implObj = ant_new CDCImplPrivate();
}

CDCImpl::CDCImpl(const char* commPort)
{
    implObj = ant_new CDCImplPrivate(commPort);
}

CDCImpl::~CDCImpl()
{
    delete implObj;
}

/*
 * Performs communication test.
 * @return TRUE if the test succeeds
 *		   FALSE otherwise
 */
bool CDCImpl::test()
{
    //flog << "test - begin:\n";

    CDCImplPrivate::Command cmd = implObj->constructCommand(MSG_TEST, uchar_str(""));
    implObj->processCommand(cmd);

    //flog << "test - end\n";
    return true;
}

void CDCImpl::resetUSBDevice()
{
    CDCImplPrivate::Command cmd = implObj->constructCommand(MSG_RES_USB, uchar_str(""));
    implObj->processCommand(cmd);
}

void CDCImpl::resetTRModule()
{
    CDCImplPrivate::Command cmd = implObj->constructCommand(MSG_RES_TR, uchar_str(""));
    implObj->processCommand(cmd);
}

void CDCImpl::indicateConnectivity()
{
    CDCImplPrivate::Command cmd = implObj->constructCommand(MSG_USB_CONN, uchar_str(""));
    implObj->processCommand(cmd);
}

DeviceInfo* CDCImpl::getUSBDeviceInfo(void)
{
    CDCImplPrivate::Command cmd = implObj->constructCommand(MSG_USB_INFO, uchar_str(""));
    implObj->processCommand(cmd);
    return implObj->msgParser->getParsedDeviceInfo(implObj->lastResponse.message);
}

ModuleInfo* CDCImpl::getTRModuleInfo(void)
{
    CDCImplPrivate::Command cmd = implObj->constructCommand(MSG_TR_INFO, uchar_str(""));
    implObj->processCommand(cmd);
    return implObj->msgParser->getParsedModuleInfo(implObj->lastResponse.message);
}

SPIStatus CDCImpl::getStatus(void)
{
    CDCImplPrivate::Command cmd = implObj->constructCommand(MSG_SPI_STAT, uchar_str(""));
    implObj->processCommand(cmd);
    return implObj->msgParser->getParsedSPIStatus(implObj->lastResponse.message);
}

DSResponse CDCImpl::sendData(const unsigned char* data, unsigned int dlen)
{
    ustring dataStr(data, dlen);
    CDCImplPrivate::Command cmd = implObj->constructCommand(MSG_DATA_SEND, dataStr);
    implObj->processCommand(cmd);
    return implObj->msgParser->getParsedDSResponse(implObj->lastResponse.message);
}

DSResponse CDCImpl::sendData(const std::basic_string<unsigned char>& data)
{
    CDCImplPrivate::Command cmd = implObj->constructCommand(MSG_DATA_SEND, data);
    implObj->processCommand(cmd);
    return implObj->msgParser->getParsedDSResponse(implObj->lastResponse.message);
}

void CDCImpl::switchToCustom(void)
{
    CDCImplPrivate::Command cmd = implObj->constructCommand(MSG_SWITCH, uchar_str(""));
    implObj->processCommand(cmd);
}


PTEResponse CDCImpl::enterProgrammingMode(void)
{
    CDCImplPrivate::Command cmd = implObj->constructCommand(MSG_MODE_PROGRAM, uchar_str(""));
    implObj->processCommand(cmd);
    return implObj->msgParser->getParsedPEResponse(implObj->lastResponse.message);
}

PTEResponse CDCImpl::terminateProgrammingMode(void)
{
    CDCImplPrivate::Command cmd = implObj->constructCommand(MSG_MODE_NORMAL, uchar_str(""));
    implObj->processCommand(cmd);
    return implObj->msgParser->getParsedPTResponse(implObj->lastResponse.message);
}

static void verifyUpload(unsigned char target, const std::basic_string<unsigned char>& data)
{
  (void)data; //silence -Wunused-parameter
  if ((target & 0x80) == 0) {
        std::ostringstream msg;
        msg << "Download target " << std::hex << std::showbase << target << " is not valid target for upload operation!";
        THROW_EXCEPT(CDCSendException, msg.str());
    }
    // TODO: Add other checks in the future
}

static void verifyDownload(unsigned char target)
{
    if ((target & 0x80) != 0) {
        std::ostringstream msg;
        msg << "Upload target " << std::hex << std::showbase << target << " is not valid target for download operation!";
        THROW_EXCEPT(CDCSendException, msg.str());
    }
    // TODO: Add other checks in the future
}

PMResponse CDCImpl::upload(unsigned char target, const unsigned char* data, unsigned int dlen)
{
    ustring dataStr(data, dlen);
    verifyUpload(target, data);
    dataStr.insert(dataStr.begin(), target);
    CDCImplPrivate::Command cmd = implObj->constructCommand(MSG_UPLOAD_DOWNLOAD, dataStr);
    implObj->processCommand(cmd);
    return implObj->msgParser->getParsedPMResponse(implObj->lastResponse.message);
}

PMResponse CDCImpl::upload(unsigned char target, const std::basic_string<unsigned char>& data)
{
    ustring dataStr = data;
    verifyUpload(target, data);
    dataStr.insert(dataStr.begin(), target);
    CDCImplPrivate::Command cmd = implObj->constructCommand(MSG_UPLOAD_DOWNLOAD, dataStr);
    implObj->processCommand(cmd);
    return implObj->msgParser->getParsedPMResponse(implObj->lastResponse.message);
}

PMResponse CDCImpl::download(unsigned char target, const unsigned char* inputData, unsigned int inputDlen,
        unsigned char* outputData, unsigned int outputDlen, unsigned int &len)
{
    ustring dataStr(inputData, inputDlen);
    len = 0;
    verifyDownload(target);
    dataStr.insert(dataStr.begin(), target);
    CDCImplPrivate::Command cmd = implObj->constructCommand(MSG_UPLOAD_DOWNLOAD, dataStr);
    implObj->processCommand(cmd);
    if (implObj->lastResponse.parseResult.msgType == MSG_DOWNLOAD_DATA) {
        dataStr = implObj->msgParser->getParsedPMData(implObj->lastResponse.message);
        if (dataStr.length() >= outputDlen) {
            std::ostringstream msg;
            msg << "Receive of download message failed. Data are longer than available data buffer - " << dataStr.length() << " >= " << outputDlen << "!";
            THROW_EXCEPT(CDCReceiveException, msg.str());
        }
        std::copy_n(dataStr.data(), dataStr.length(), outputData);
        len = static_cast<unsigned int>(dataStr.length());
        return PMResponse::OK;
    } else {
        return implObj->msgParser->getParsedPMResponse(implObj->lastResponse.message);
    }
}

PMResponse CDCImpl::download(unsigned char target, const std::basic_string<unsigned char>& inputData,
        std::basic_string<unsigned char>& outputData)
{
    ustring dataStr = inputData;
    verifyDownload(target);
    dataStr.insert(dataStr.begin(), target);
    CDCImplPrivate::Command cmd = implObj->constructCommand(MSG_UPLOAD_DOWNLOAD, dataStr);
    implObj->processCommand(cmd);
    if (implObj->lastResponse.parseResult.msgType == MSG_DOWNLOAD_DATA) {
        dataStr = implObj->msgParser->getParsedPMData(implObj->lastResponse.message);
        outputData = dataStr;
        return PMResponse::OK;
    } else {
        return implObj->msgParser->getParsedPMResponse(implObj->lastResponse.message);
    }
}

bool CDCImpl::isReceptionStopped(void)
{
    return implObj->getReceptionStopped();
}

/*
 * Returns last reception error.
 */
std::string CDCImpl::getLastReceptionError(void)
{
    return implObj->cloneLastReceptionError();
}

/* Registers user-defined listener of asynchronous messages. */
void CDCImpl::registerAsyncMsgListener(AsyncMsgListenerF asyncListener)
{
    implObj->setAsyncListener(asyncListener);
}

/* Unregisters listener of asynchronous messages. */
void CDCImpl::unregisterAsyncMsgListener(void)
{
    implObj->setAsyncListener(AsyncMsgListenerF());
}

//////////////////////////////////////
// class CDCImplPrivate
//////////////////////////////////////
/*
* Creates instance with COM-port set to COM1.
*/
CDCImplPrivate::CDCImplPrivate()
{
    init();
}

/*
* Creates instance with specified COM-port.
* @param commPort COM-port to communicate with
*/
CDCImplPrivate::CDCImplPrivate(const char* portName)
  :m_commPort(portName)
{
    init();
}

/* Encapsulates basic initialization process. */
void CDCImplPrivate::init()
{
    //createNewLogFile();
    m_transmitBuffer = ant_new unsigned char[1024];;
    m_transmitBufferLen = 1024;

    portHandle = openPort(m_commPort);

    createMyEvent(newMsgEvent);
    createMyEvent(readEndEvent);
    createMyEvent(readStartEvent);
    createMyEvent(readEndResponse);

    initMessageHeaders();
    initLastResponse();

    receptionStopped = false;

    msgParser = ant_new CDCMessageParser();

    resetMyEvent(readStartEvent);

    readMsgHandle = std::thread(&CDCImplPrivate::readMsgThread, this);

    // waiting for reading thread
    waitForMyEvent(readStartEvent, TM_START_READ);
}

/*
* Destroys communication object and frees all needed resources.
*/
CDCImplPrivate::~CDCImplPrivate()
{
    setMyEvent(readEndEvent);

  //TODO cancel join?
//  int joinResult = 0;
//  int waitResult = waitForEvent(readEndResponse, TM_CANCEL_READ);
//  switch (waitResult) {
//      case -1:
//          std::cerr << "Waiting for read end event failed " << errno << "\n";
//          goto CLEAR;
//      case 0:
//          std::cerr << "Waiting for read end event was timeouted\n";
//			goto CLEAR;
//      default:
//          break;
//  }

    if (readMsgHandle.joinable())
        readMsgHandle.join();

    destroyMyEvent(readStartEvent);
    destroyMyEvent(newMsgEvent);
    destroyMyEvent(readEndEvent);
    destroyMyEvent(readEndResponse);

    closePort(portHandle);

    delete msgParser;
    delete[] m_transmitBuffer;

    //flog.close();
}

/* Initializes messageHeaders map. */
void CDCImplPrivate::initMessageHeaders(void)
{
    messageHeaders.insert(pair<MessageType, string>(MSG_TEST, "OK"));
    messageHeaders.insert(pair<MessageType, string>(MSG_RES_USB, "R"));
    messageHeaders.insert(pair<MessageType, string>(MSG_RES_TR, "RT"));
    messageHeaders.insert(pair<MessageType, string>(MSG_USB_INFO, "I"));
    messageHeaders.insert(pair<MessageType, string>(MSG_TR_INFO, "IT"));
    messageHeaders.insert(pair<MessageType, string>(MSG_USB_CONN, "B"));
    messageHeaders.insert(pair<MessageType, string>(MSG_SPI_STAT, "S"));
    messageHeaders.insert(pair<MessageType, string>(MSG_DATA_SEND, "DS"));
    messageHeaders.insert(pair<MessageType, string>(MSG_SWITCH, "U"));
    messageHeaders.insert(pair<MessageType, string>(MSG_ASYNC, "DR"));
    messageHeaders.insert(pair<MessageType, string>(MSG_MODE_NORMAL, "PT"));
    messageHeaders.insert(pair<MessageType, string>(MSG_MODE_PROGRAM, "PE"));
    messageHeaders.insert(pair<MessageType, string>(MSG_UPLOAD_DOWNLOAD, "PM"));
    messageHeaders.insert(pair<MessageType, string>(MSG_DOWNLOAD_DATA, "PM")); // Used only by receive operation
}

void CDCImplPrivate::initLastResponse(void)
{
    lastResponse.message = ustring(uchar_str(""));
    lastResponse.parseResult.msgType = MSG_ERROR;
    lastResponse.parseResult.resultType = PARSE_NOT_COMPLETE;
    lastResponse.parseResult.lastPosition = 0;
}

void CDCImplPrivate::setAsyncListener(AsyncMsgListenerF listener)
{
    std::lock_guard<std::mutex> lck(csAsyncListener);
    asyncListener = listener;
}

bool CDCImplPrivate::getReceptionStopped(void)
{
    std::lock_guard<std::mutex> lck(csReadingStopped);
    bool tmpReadingStopped = receptionStopped;
    return tmpReadingStopped;
}

void CDCImplPrivate::setReceptionStopped(bool value)
{
    std::lock_guard<std::mutex> lck(csReadingStopped);
    receptionStopped = value;
}

/* Sets last reception error according to parameters. */
void CDCImplPrivate::setLastReceptionError(const std::string& descr)
{
    std::lock_guard<std::mutex> lck(csLastRecpError);
    lastReceptionError = descr;
}

/* Clones last reception error. */
std::string CDCImplPrivate::cloneLastReceptionError()
{
    std::lock_guard<std::mutex> lck(csLastRecpError);
    std::string ret(lastReceptionError.c_str());
    return ret;
}

/*
* Process specified message. First of all, the message is parsed.
* If the message is asynchronous message, then  registered listener(if exists)
* is called. Otherwise, last response is updated and "anew message"
* signal for main thread is set.
* @throw CDCReceiveException
*/
void CDCImplPrivate::processMessage(ParsedMessage& parsedMessage)
{
    if (parsedMessage.parseResult.msgType == MSG_ASYNC) {
        std::lock_guard<std::mutex> lck(csAsyncListener);
        if (asyncListener != NULL) {
            ustring userData = msgParser->getParsedDRData(parsedMessage.message);

            unsigned char* userDataBytes = ant_new unsigned char[userData.length() + 1];
            userData.copy(userDataBytes, userData.length());
            userDataBytes[userData.length()] = '\0';

            asyncListener(userDataBytes, static_cast<unsigned int>(userData.length()));
            delete[] userDataBytes;
        }

        return;
    }

    // copy last parsed message into last response
    lastResponse.parseResult = parsedMessage.parseResult;
    lastResponse.message = parsedMessage.message;

    setMyEvent(newMsgEvent);
}

/*
* Extracts and processes all messages inside the specified buffer.
* @throw CDCReading Exception
*/
void CDCImplPrivate::processAllMessages(ustring& msgBuffer)
{
    if (msgBuffer.empty())
        return;

    ParsedMessage parsedMessage = parseNextMessage(msgBuffer);
    while (parsedMessage.parseResult.resultType != PARSE_NOT_COMPLETE) {
        if (parsedMessage.parseResult.resultType == PARSE_BAD_FORMAT) {
            // throw all bytes from the buffer up to next 0x0D
            size_t endMsgPos = msgBuffer.find(0x0D, parsedMessage.parseResult.lastPosition);
            if (endMsgPos == string::npos)
                msgBuffer.clear();
            else
                msgBuffer.erase(0, endMsgPos + 1);

            setLastReceptionError("Bad message format");
        } else {
            msgBuffer.erase(0, parsedMessage.parseResult.lastPosition + 1);
            processMessage(parsedMessage);
        }

        if (msgBuffer.empty())
            return;

        parsedMessage = parseNextMessage(msgBuffer);
    }
}

/*
* Extracts message string from specified buffer and returns
* it in the form of string. If the buffer does not contain
* full message, empty string is returned.
*/
CDCImplPrivate::ParsedMessage CDCImplPrivate::parseNextMessage(ustring& msgBuffer)
{
    ParsedMessage parsedMessage;
    ustring parsedMsg;

    // Bugfix of error in fw implementation
    if (msgBuffer.length() > 0 && msgBuffer[0] == '>')
        msgBuffer[0] = '<';

    ParseResult parseResult = msgParser->parseData(msgBuffer);

    switch (parseResult.resultType) {
    case PARSE_OK:
        parsedMsg = msgBuffer.substr(0, parseResult.lastPosition + 1);
        parsedMessage.message = parsedMsg;
        break;

    case PARSE_NOT_COMPLETE:
        parsedMessage.message = ustring(uchar_str(""));
        break;

    case PARSE_BAD_FORMAT:
        parsedMessage.message = ustring(uchar_str(""));
        break;
    }

    parsedMessage.parseResult = parseResult;
    return parsedMessage;
}

/*
* Construct command and returns it.
* @return command of specified message type with data.
*/
CDCImplPrivate::Command CDCImplPrivate::constructCommand(MessageType msgType, ustring data)
{
    //flog << "implObj->constructCommand - begin:\n";

    Command cmd;
    cmd.msgType = msgType;
    cmd.data = data;

    //flog << "implObj->constructCommand - end\n" ;
    return cmd;
}

/*
* Bufferize specified command, so that it can be passed to COM-port.
* @param cmd command to bufferize
* @return bufferrized form of command
*/
CDCImplPrivate::BuffCommand CDCImplPrivate::commandToBuffer(Command& cmd)
{
    ustring tmpStr(uchar_str(">"));
    if (cmd.msgType != MSG_TEST)
        tmpStr.append(uchar_str(messageHeaders[cmd.msgType].c_str()));

    if (cmd.msgType == MSG_DATA_SEND) {
        if (cmd.data.size() > UCHAR_MAX)
            THROW_EXCEPT(CDCSendException, "Data size too large");

        tmpStr.append(1, static_cast<unsigned char>(cmd.data.size()));

        // appending data length in hex format
        /*
        stringstream strStream;
        strStream << hex << cmd.data.size();
        string dataLenStr = strStream.str();
        if (dataLenStr.size() == 1) {
        tmpStr.append("0");
        }

        tmpStr.append(dataLenStr);
        */
        tmpStr.append(uchar_str(":"));
        tmpStr.append(cmd.data);
    }

    if (cmd.msgType == MSG_UPLOAD_DOWNLOAD || cmd.msgType == MSG_DOWNLOAD_DATA) {
        if (cmd.data.size() > UCHAR_MAX)
            THROW_EXCEPT(CDCSendException, "Data size too large");
        tmpStr.append(cmd.data);
    }

    tmpStr.append(1, 0x0D);

    size_t sz = tmpStr.size();
    if (m_transmitBufferLen < sz) { //reallocate
        delete[] m_transmitBuffer;
        m_transmitBuffer = ant_new unsigned char[sz];
        m_transmitBufferLen = static_cast<DWORD>(sz);
    }

    BuffCommand buffCmd;
    buffCmd.cmd = m_transmitBuffer;
    tmpStr.copy(buffCmd.cmd, sz);
    buffCmd.len = static_cast<DWORD>(sz);

    return buffCmd;
}

/*
* Checks, if specified value is the correct value of SPIStatus.
* @param statValue string value to check for
* @return TRUE stat value is correct value of SPIStatus
*		   FALSE otherwise
*/
//bool CDCImplPrivate::isSPIStatusValue(ustring& statValue) {
//  int parsedValue = strtol((const char*)statValue.c_str(), NULL, 16);
//  if (parsedValue <= DISABLED && parsedValue <= HW_ERROR) {
//    return true;
//  }
//
//  return false;
//}

/*
* Sends command, waits for response a checks the response.
* @param cmd command to process.
* @throw CDCImplException if some error occurs during processing
*/
void CDCImplPrivate::processCommand(Command& cmd)
{
    if (getReceptionStopped())
        THROW_EXCEPT(CDCSendException, "Reading is actually stopped")

    sendCommand(cmd);
    //wait for response
    waitForMyEvent(newMsgEvent, TM_WAIT_RESP);

    if (lastResponse.parseResult.msgType != cmd.msgType) {
        // TODO: Find some better way to solve upload/download duality
        if (!((cmd.msgType == MSG_UPLOAD_DOWNLOAD)
                && (lastResponse.parseResult.msgType == MSG_DOWNLOAD_DATA)
                && ((cmd.data[0] & 0x80) == 0)))
        {
            THROW_EXCEPT(CDCReceiveException, "Response has bad type.");
        }
    }
}

/*
// name of file to log into
const char* LOG_FILE = "cdclib.log";


// file logging object
fstream flog;

// creates logging file
void createNewLogFile() {
flog.open(LOG_FILE);
if (flog.fail() || flog.bad()) {
cerr << "Logging initialization failed, error: " << GetLastError() << "\n";
throw CDCImplException("Error while openning logging file ");
} else {
cout << "Logging init started\n";
}

flog << "Initialization started\n";
}
*/
