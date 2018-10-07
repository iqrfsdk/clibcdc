/*
 * Copyright 2018 MICRORISC s.r.o.
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

#include <string>
#include <set>
#include <map>
#include <mutex>
#include <sstream>
#include <CDCMessageParser.h>
#include <CDCMessageParserException.h>

/*
 * Implementation class.
 */
class CDCMessageParserPrivate {
public:
	CDCMessageParserPrivate();
	~CDCMessageParserPrivate();

	/* Information about state. */
	struct StateInfo {
		MessageType msgType;    // associated message type
		bool multiType;			// if more message types are possible

	};

	/* Associates state and input. */
	struct StateInputPair {
		unsigned int stateId;
		unsigned int input;
	};

	/* Comparison object of 2 States. */
	struct StateInputPairCompare {
		bool operator() (const StateInputPair& lhs, const StateInputPair& rhs) const
		{
			if (lhs.stateId != rhs.stateId) {
				return lhs.stateId < rhs.stateId;
			}
			return lhs.input < rhs.input;
		}
	};

	/* Results of processing of special state. */
	struct StateProcResult {
		unsigned int newState;		// ant_new state
		unsigned int lastPosition;  // last processed position
		bool formatError;           // indication of format error
	};

	typedef std::set<unsigned int> setOfStates;

	typedef std::map<unsigned int, StateInfo> mapOfStates;

	typedef std::map<StateInputPair, unsigned int, StateInputPairCompare> stateInputToStateMap;


    // map of information about each state
    mapOfStates statesInfoMap;

	// set of finite states
	setOfStates finiteStates;

	// states, which require special processing
	setOfStates specialStates;

	// map of all transitions between states
	stateInputToStateMap transitionMap;

    // last parsed data
	ustring lastParsedData;

	// last parse result information
	ParseResult lastParseResult;

    /* Set of all values of SPIModes. */
	std::set<SPIModes> spiModes;


	/* INITIALIZATION.*/
	void insertStatesInfo(unsigned int states[], unsigned int statesSize,
		MessageType msgType);

	/* Inserting states, which have multiple message types associated with. */
    void insertMultiTypeStatesInfo(unsigned int states[], unsigned int statesSize);

    void initStatesInfoMap(void);

	void insertTransition(unsigned int stateId, unsigned int input,
		unsigned int nextStateId);

	/* Inits transition map. */
	void initTransitionMap(void);

	/* Inits finite states. */
	void initFiniteStates(void);

	/* Inits special states. */
    void initSpecialStates(void);

	/* Initializes spiModes set. */
	void initSpiModes(void);


	/* SPECIAL STATES PROCESSING. */
	/* Processes state 17. */
	StateProcResult processUSBInfo(ustring& data, unsigned int pos);

    /* Processes state 21. */
	StateProcResult processTRInfo(ustring& data, unsigned int pos);

    /* Processes state 50. */
	StateProcResult processAsynData(ustring& data, unsigned int pos);

	/* Processes state 95. */
	StateProcResult processPMRespData(ustring& data, unsigned int pos);

	 /* Switch function of processing some special state. */
	StateProcResult processSpecialState(unsigned int state, ustring& data,
		unsigned int pos);



	/* Indicates, wheather specified state is final state. */
	bool isFiniteState(unsigned int state);

    /* Indicates, wheather specified state is special state. */
	bool isSpecialState(unsigned int state);

	/* Returns state after specified transition. */
	unsigned int doTransition(unsigned int state, unsigned char input);

	/* Parses specified data. */
	ParseResult parseData(ustring& data);
};


/* Initial state. */
const unsigned int INITIAL_STATE = 0;

/* Indicates, that there is no transition possible. */
const unsigned int NO_TRANSITION = 65535;

/* All inputs - something like '*' in reg. expressions. */
const unsigned int INPUT_ALL = 1000;


// critical section for thread safe access public interface methods
//CRITICAL_SECTION csUI;
std::mutex mtxUI;

/*
 * For converting string literals to unsigned string literals.
 */
inline const unsigned char* uchar_str(const char* s){
  return reinterpret_cast<const unsigned char*>(s);
}

/*
 * Indicates, wheather specified state is final state.
 */
bool CDCMessageParserPrivate::isFiniteState(unsigned int state) {
	setOfStates::iterator statesIt = finiteStates.find(state);
	if (statesIt == finiteStates.end()) {
		return false;
	}

	return true;
}

/*
 * Indicates, wheather specified state is special state.
 */
bool CDCMessageParserPrivate::isSpecialState(unsigned int state) {
	setOfStates::iterator statesIt = specialStates.find(state);
	if (statesIt == specialStates.end()) {
		return false;
	}

	return true;
}

/*
 * Returns state after specified transition.
 * If no transition exists, return NO_TRANSITION.
 */
unsigned int CDCMessageParserPrivate::doTransition(unsigned int state,
		unsigned char input) {
	StateInputPair stateInput = { state, input };

	stateInputToStateMap::iterator nextStateIt = transitionMap.find(stateInput);
	if (nextStateIt != transitionMap.end()) {
		return nextStateIt->second;
	}

    StateInputPair stateInputAll = { state, INPUT_ALL };
	stateInputToStateMap::iterator nextStateAllIt = transitionMap.find(stateInputAll);
	if (nextStateAllIt != transitionMap.end()) {
		return nextStateAllIt->second;
	}

	return NO_TRANSITION;
}

/*
 * Inserts states, which all have specified message type associated with.
 */
void CDCMessageParserPrivate::insertStatesInfo(unsigned int states[], unsigned int
		statesSize, MessageType msgType) {
	for (unsigned int i = 0; i < statesSize; i++) {
		StateInfo stateInfo = { msgType, false };
		statesInfoMap.insert(std::pair<unsigned int, StateInfo>(states[i], stateInfo));
	}
}

/* Inserts states, which all have more message types associated with. */
void CDCMessageParserPrivate::insertMultiTypeStatesInfo(unsigned int states[],
		unsigned int statesSize) {
    for (unsigned int i = 0; i < statesSize; i++) {
		StateInfo stateInfo = { MSG_ERROR, true };
		statesInfoMap.insert(std::pair<unsigned int, StateInfo>(states[i], stateInfo));
	}
}

/* Inits states info map. */
void CDCMessageParserPrivate::initStatesInfoMap(void) {
	unsigned int multiTypeStates[] = { 0, 1, 9, 16, 33, 58, 79, 95};
	insertMultiTypeStatesInfo(multiTypeStates, 8);

	unsigned int errStates[] = { 2, 3, 4, 5 };
	insertStatesInfo(errStates, 4, MSG_ERROR);

	unsigned int testStates[] = { 6, 7, 8 };
	insertStatesInfo(testStates, 3, MSG_TEST);

	unsigned int resUsbStates[] = { 10, 101, 102, 103 };
	insertStatesInfo(resUsbStates, 4, MSG_RES_USB);

	unsigned int resTRStates[] = { 11, 12, 13, 14, 15 };
	insertStatesInfo(resTRStates, 5, MSG_RES_TR);

	unsigned int usbInfoStates[] = { 17, 18, 19 };
	insertStatesInfo(usbInfoStates, 3, MSG_USB_INFO);

	unsigned int trInfoStates[] = { 20, 21, 22, 23 };
	insertStatesInfo(trInfoStates, 4, MSG_TR_INFO);

	unsigned int usbConStates[] = { 24, 25, 26, 27, 28 };
	insertStatesInfo(usbConStates, 5, MSG_USB_CONN);

	unsigned int spiStatusStates[] = { 29, 30, 31, 32 };
	insertStatesInfo(spiStatusStates, 4, MSG_SPI_STAT);

	unsigned int dataSendStates[] = {	34, 35, 36, 37, 38,
										39, 40, 41, 42, 43,
										44, 45, 46, 47  };
	insertStatesInfo(dataSendStates, 14, MSG_DATA_SEND);

	unsigned int dataReceiveStates[] = { 48, 49, 50, 51, 52 };
	insertStatesInfo(dataReceiveStates, 5, MSG_ASYNC);

	unsigned int switchStates[] = { 53, 54, 55, 56, 57 };
	insertStatesInfo(switchStates, 5,  MSG_SWITCH);

	unsigned int modeNormal[] = { 69, 70, 71, 72, 73, 74, 75, 76, 77, 78 };
	insertStatesInfo(modeNormal, 10,  MSG_MODE_NORMAL);

	unsigned int modeProgram[] = { 59, 60, 61, 62, 63, 64, 65, 66, 67, 68 };
	insertStatesInfo(modeProgram, 10,  MSG_MODE_PROGRAM);

	unsigned int resUploadDownload[] = { 80, 81, 82, 83, 84, 85, 86,
                                             87, 88, 89, 90, 91, 92, 93};
	insertStatesInfo(resUploadDownload, 17,  MSG_UPLOAD_DOWNLOAD);

        unsigned int dataDownload[] = { 96, 97 };
	insertStatesInfo(dataDownload, 2,  MSG_DOWNLOAD_DATA);
}

/* Inserts transition into transition map. */
void CDCMessageParserPrivate::insertTransition(unsigned int stateId, unsigned int input,
		unsigned int nextStateId) {
	StateInputPair inputPair = { stateId, input };
	transitionMap.insert(std::pair<StateInputPair, int>(inputPair, nextStateId));
}

/* Inits transition map. */
void CDCMessageParserPrivate::initTransitionMap(void) {
    // beginning
	insertTransition(0, '<', 1);
	insertTransition(1, 'E', 2);
	insertTransition(1, 'O', 6);
	insertTransition(1, 'R', 9);
	insertTransition(1, 'I', 16);
	insertTransition(1, 'B', 24);
	insertTransition(1, 'S', 29);
	insertTransition(1, 'D', 33);
	insertTransition(1, 'U', 53);
	insertTransition(1, 'P', 58);

	// ERR
	insertTransition(2, 'R', 3);
	insertTransition(3, 'R', 4);
	insertTransition(4, 0x0D, 5);

	// MSG_TEST OK
	insertTransition(6, 'K', 7);
	insertTransition(7, 0x0D, 8);

	// RESET USB
    insertTransition(9, ':', 10);
    insertTransition(10, 'O', 101);
    insertTransition(101, 'K', 102);
    insertTransition(102, 0x0D, 103);

	// RESET MODULE
    insertTransition(9, 'T', 11);
	insertTransition(11, ':', 12);
	insertTransition(12, 'O', 13);
	insertTransition(13, 'K', 14);
	insertTransition(14, 0x0D, 15);

	// USB INFO
	insertTransition(16, ':', 17);

	// handled via special function
	//insertTransition(17, text, 18);
	insertTransition(18, 0x0D, 19);

	// MODULE INFO
	insertTransition(16, 'T', 20);
	insertTransition(20, ':', 21);

	// handled via special function
	//insertTransition(21, text, 22);
	insertTransition(22, 0x0D, 23);

    // USB CONNECTION INDICATION
	insertTransition(24, ':', 25);
	insertTransition(25, 'O', 26);
	insertTransition(26, 'K', 27);
	insertTransition(27, 0x0D, 28);

	// SPI STATUS
	insertTransition(29, ':', 30);
	insertTransition(30, INPUT_ALL, 31);
	insertTransition(31, 0x0D, 32);

    // DATA SEND
	insertTransition(33, 'S', 34);
	insertTransition(34, ':', 35);
	insertTransition(35, 'O', 36);
	insertTransition(36, 'K', 37);
	insertTransition(37, 0x0D, 38);

	insertTransition(35, 'E', 39);
	insertTransition(39, 'R', 40);
	insertTransition(40, 'R', 41);
	insertTransition(41, 0x0D, 42);

	insertTransition(35, 'B', 43);
	insertTransition(43, 'U', 44);
	insertTransition(44, 'S', 45);
	insertTransition(45, 'Y', 46);
	insertTransition(46, 0x0D, 47);

	// DATA RECEIVE
    insertTransition(33, 'R', 48);
	insertTransition(48, INPUT_ALL, 49);
	insertTransition(49, ':', 50);

	// handled via special function
	//insertTransition(50, data, 51);
	insertTransition(51, 0x0D, 52);

    // CDC SWITCH
    insertTransition(53, ':', 54);
    insertTransition(54, 'O', 55);
    insertTransition(55, 'K', 56);
	insertTransition(56, 0x0D, 57);

	// Programming
	insertTransition(58, 'E', 59);
	insertTransition(58, 'T', 69);
	insertTransition(58, 'M', 79);

	// Enter programming mode
	insertTransition(59, ':', 60);
	insertTransition(60, 'O', 61);
	insertTransition(60, 'E', 64);
	insertTransition(61, 'K', 62);
	insertTransition(62, 0x0D, 63);
	insertTransition(64, 'R', 65);
	insertTransition(65, 'R', 66);
	insertTransition(66, '1', 67);
	insertTransition(67, 0x0D, 68);

	// Terminate programming mode
	insertTransition(69, ':', 70);
	insertTransition(70, 'O', 71);
	insertTransition(70, 'E', 74);
	insertTransition(71, 'K', 72);
	insertTransition(72, 0x0D, 73);
	insertTransition(74, 'R', 75);
	insertTransition(75, 'R', 76);
	insertTransition(76, '1', 77);
	insertTransition(77, 0x0D, 78);

	// Upload/Download
	// handled via special function
	insertTransition(79, ':', 95);
	//insertTransition(79, data, 96);

	// Upload/Error
	insertTransition(80, 'O', 81);
	insertTransition(80, 'E', 84);
    insertTransition(80, 'B', 89);
	insertTransition(81, 'K', 82);
	insertTransition(82, 0x0D, 83);
	insertTransition(84, 'R', 85);
	insertTransition(85, 'R', 86);
	insertTransition(86, '2', 87);
	insertTransition(86, '3', 87);
	insertTransition(86, '4', 87);
	insertTransition(86, '5', 87);
	insertTransition(86, '6', 87);
	insertTransition(86, '7', 87);
	insertTransition(87, 0x0D, 88);
	insertTransition(89, 'U', 90);
	insertTransition(90, 'S', 91);
	insertTransition(91, 'Y', 92);
	insertTransition(92, 0x0D, 93);

	// Download Data
	insertTransition(96, 0x0D, 97);
}

/* Inits finite states set. */
void CDCMessageParserPrivate::initFiniteStates(void) {
	finiteStates.insert(5);
	finiteStates.insert(8);
	finiteStates.insert(15);
	finiteStates.insert(19);
	finiteStates.insert(23);
	finiteStates.insert(28);
	finiteStates.insert(32);
	finiteStates.insert(38);
	finiteStates.insert(42);
	finiteStates.insert(47);
	finiteStates.insert(52);
	finiteStates.insert(57);
	finiteStates.insert(103);
	finiteStates.insert(63);
	finiteStates.insert(68);
	finiteStates.insert(73);
	finiteStates.insert(78);
	finiteStates.insert(83);
	finiteStates.insert(88);
	finiteStates.insert(93);
	finiteStates.insert(97);
}

/* Inits finite states set. */
void CDCMessageParserPrivate::initSpecialStates(void) {
	specialStates.insert(17);
	specialStates.insert(21);
	specialStates.insert(50);
        specialStates.insert(95);
}

/* Initializes spiModes set. */
void CDCMessageParserPrivate::initSpiModes() {
	spiModes.insert(DISABLED);
	spiModes.insert(SUSPENDED);
	spiModes.insert(BUFF_PROTECT);
	spiModes.insert(CRCM_ERR);
	spiModes.insert(READY_COMM);
	spiModes.insert(READY_PROG);
	spiModes.insert(READY_DEBUG);
	spiModes.insert(SLOW_MODE);
	spiModes.insert(HW_ERROR);
}

CDCMessageParserPrivate::CDCMessageParserPrivate() {
	initStatesInfoMap();
	initTransitionMap();
	initFiniteStates();
	initSpecialStates();
	initSpiModes();

	lastParseResult.msgType = MSG_ERROR;
	lastParseResult.resultType = PARSE_NOT_COMPLETE;
	lastParseResult.lastPosition = 0;
}

CDCMessageParserPrivate::~CDCMessageParserPrivate() {
	specialStates.clear();
	finiteStates.clear();
	transitionMap.clear();
	statesInfoMap.clear();
	spiModes.clear();
}


bool checkUSBDeviceType(unsigned char byteToCheck) {
	return true;
}



bool checkUSBDeviceVersion(unsigned char byteToCheck) {
	if ((byteToCheck >= '0') && (byteToCheck <= '9')) {
		return true;
	}

	if (byteToCheck == '.') {
		return true;
	}

	return false;
}

bool checkUSBDeviceId(unsigned char byteToCheck) {
	if ((byteToCheck >= '0') && (byteToCheck <= '9')) {
		return true;
	}
    if ( ( byteToCheck >= 'A' ) && ( byteToCheck <= 'H' ) ) {
		return true;
	}

	return false;
}


CDCMessageParserPrivate::StateProcResult CDCMessageParserPrivate::processUSBInfo(ustring& data,
		unsigned int pos) {
	StateProcResult procResult = { 17, pos, false };

	if (pos == (data.size() - 1)) {
		return procResult;
	}

	const unsigned int TYPE = 0;
	const unsigned int VERSION = 1;
	const unsigned int ID = 2;

	unsigned int activeSection = TYPE;
	procResult.newState = 18;

	for (unsigned int i = pos; i < data.size(); i++) {
		procResult.lastPosition = i;

		if (data[i] == 0x0D) {
			if (activeSection == ID) {
				procResult.newState = 19;
				break;
			}
		}

		if (data[i] == '#') {
			if (activeSection == TYPE) {
				activeSection = VERSION;
			} else if (activeSection == VERSION) {
				activeSection = ID;
			} else {
				procResult.formatError = true;
				break;
			}

			continue;
		}

		switch (activeSection) {
			case TYPE:
				if (!checkUSBDeviceType(data[i])) {
                	procResult.formatError = true;
				}
			break;

			case VERSION:
				if (!checkUSBDeviceVersion(data[i])) {
                	procResult.formatError = true;
				}
			break;

			case ID:
				if (!checkUSBDeviceId(data[i])) {
                	procResult.formatError = true;
				}
			break;
		}

		if (procResult.formatError) {
        	break;
		}
	}

	return procResult;
}

/* Processes state 21. */
CDCMessageParserPrivate::StateProcResult CDCMessageParserPrivate::processTRInfo(ustring& data,
		unsigned int pos) {

    const unsigned int MODULE_DATA_SIZE = 32;
    const unsigned int STANDARD_IDF_SIZE = 21;
    const unsigned int EXTENDED_IDF_SIZE = 37;

	StateProcResult procResult = { 21, pos, false };

	if (pos == (data.size() - 1)) {
		return procResult;
	}

    if (data.size() <= EXTENDED_IDF_SIZE) {
        if (data.size() != STANDARD_IDF_SIZE && data.size() != EXTENDED_IDF_SIZE) {
            return procResult;
        }
        else {
            if (data.size() == STANDARD_IDF_SIZE && data.at(STANDARD_IDF_SIZE - 1) != 0x0D) {
                return procResult;
            }
        }
    }

	procResult.newState = 22;

	if ((data.size()-1) > (pos + MODULE_DATA_SIZE)) {
		procResult.lastPosition = pos-1 + MODULE_DATA_SIZE;
	} else {
      	procResult.lastPosition = data.size()-2;
	}

	return procResult;
}

/* Processes state 50. */
CDCMessageParserPrivate::StateProcResult CDCMessageParserPrivate::processAsynData(ustring& data,
		unsigned int pos) {
	StateProcResult procResult = { 50, pos, false };

	if (pos == (data.size() - 1)) {
		return procResult;
	}

	procResult.newState = 51;
	unsigned int dataLength = data.at(pos-2);

	if ((pos + dataLength) >= data.size()) {
		procResult.lastPosition = data.size()-1;
	} else {
		procResult.lastPosition = (pos-1) + dataLength;
	}

	return procResult;
}

/* Processes state 95. Heuristic - error/upload message or valid download data */
CDCMessageParserPrivate::StateProcResult CDCMessageParserPrivate::processPMRespData(ustring& data,
		unsigned int pos) {
	StateProcResult procResult = { 95, pos, false };

	if (pos == (data.size() - 1)) {
		return procResult;
	}

        // Check length of message with error codes
        if (data.size() == 7 || data.size() == 9) {
                // Error message
                procResult.newState = 80;
                procResult.lastPosition = pos - 1;
        } else {
                // Message length should be 5 or 36. However, we threat
                // all message lengths other than 7 and 9 as download data.
                procResult.newState = 96;
                procResult.lastPosition = data.size()-2;
        }

	return procResult;
}

/*
 * Processes specified special state.
 */
CDCMessageParserPrivate::StateProcResult CDCMessageParserPrivate::processSpecialState(
		unsigned int state, ustring& data, unsigned int pos) {
	switch (state) {
		case 17:
			return processUSBInfo(data, pos);
		case 21:
			return processTRInfo(data, pos);
		case 50:
			return processAsynData(data, pos);
		case 95:
			return processPMRespData(data, pos);
	}

	// error - invalid parser state
	std::stringstream excStream;
	excStream << "Unknown special state: " << state;
	throw CDCMessageParserException((excStream.str()).c_str());
}

ParseResult CDCMessageParserPrivate::parseData(ustring& data) {
	lastParsedData = data;
	lastParseResult.resultType = PARSE_NOT_COMPLETE;
	unsigned int state = INITIAL_STATE;

	for (unsigned int pos = 0; pos < lastParsedData.size(); pos++) {
		lastParseResult.lastPosition = pos;

		// special handling of some states
		if (isSpecialState(state)) {
			StateProcResult procResult = processSpecialState(state,
				lastParsedData, pos);
            lastParseResult.lastPosition = procResult.lastPosition;
			if (procResult.formatError) {
				lastParseResult.resultType = PARSE_BAD_FORMAT;
				return lastParseResult;
			} else {
				state = procResult.newState;
				pos = procResult.lastPosition;

				// in the case of final state, return related message type
				if (isFiniteState(state)) {
					mapOfStates::iterator stateInfoIt = statesInfoMap.find(state);
					lastParseResult.msgType = stateInfoIt->second.msgType;
					lastParseResult.resultType = PARSE_OK;
					return lastParseResult;
				}
			}

			continue;
		}

		// do transition to next state
		state = doTransition(state, lastParsedData[pos]);
		if (state == NO_TRANSITION) {
			lastParseResult.resultType = PARSE_BAD_FORMAT;
			return lastParseResult;
		}

		// in the case of final state, return related message type
		if (isFiniteState(state)) {
			mapOfStates::iterator stateInfoIt = statesInfoMap.find(state);
			lastParseResult.msgType = stateInfoIt->second.msgType;
			lastParseResult.resultType = PARSE_OK;
			return lastParseResult;
		}
	}

	return lastParseResult;
}


/* PUBLIC INTERFACE. */
CDCMessageParser::CDCMessageParser() {
	implObj = ant_new CDCMessageParserPrivate();
	//InitializeCriticalSection(&csUI);
}

CDCMessageParser::~CDCMessageParser() {
	delete implObj;
	//DeleteCriticalSection(&csUI);
}

ParseResult CDCMessageParser::parseData(ustring& data) {
	std::lock_guard<std::mutex> lck(mtxUI);	//EnterCriticalSection(&csUI);

	ParseResult parseResult = implObj->parseData(data);

	//LeaveCriticalSection(&csUI);
	return parseResult;
}

DeviceInfo* CDCMessageParser::getParsedDeviceInfo(ustring& data) {
	std::lock_guard<std::mutex> lck(mtxUI);	//EnterCriticalSection(&csUI);

	DeviceInfo* devInfo = ant_new DeviceInfo();

	// type parsing
	size_t firstHashPos = data.find('#', 3);
	size_t typeSize = firstHashPos - 3;
	ustring typeStr = data.substr(3, typeSize);

	devInfo->type = ant_new char[typeSize + 1];
	typeStr.copy ((unsigned char*)devInfo->type, typeStr.size()); //strcpy(devInfo->type, (const char*)typeStr.c_str());
	devInfo->typeLen = typeSize;

	// firmware version parsing
    size_t secondHashPos = data.find('#', firstHashPos+1);
    size_t fmSize = secondHashPos - firstHashPos - 1;
	ustring fmStr = data.substr(firstHashPos+1, fmSize);

	devInfo->firmwareVersion = ant_new char[fmSize + 1];
	fmStr.copy ((unsigned char*)devInfo->firmwareVersion, fmStr.size()); //strcpy(devInfo->firmwareVersion, (const char*)fmStr.c_str());
	devInfo->fwLen = fmSize;

    // serial number parsing
	size_t crPos = data.find(13, secondHashPos+1);
    size_t snSize = crPos - secondHashPos - 1;
	ustring snStr = data.substr(secondHashPos+1, snSize);

    devInfo->serialNumber = ant_new char[snSize + 1];
    snStr.copy ((unsigned char*)devInfo->serialNumber, snStr.size()); //strcpy(devInfo->serialNumber, (const char*)snStr.c_str());
	devInfo->snLen = snSize;

	//LeaveCriticalSection(&csUI);
	return devInfo;
}

ModuleInfo* CDCMessageParser::getParsedModuleInfo(ustring& data) {
    #define STANDARD_IDF_SIZE   21
    #define EXTENDED_IDF_SIZE   37

	std::lock_guard<std::mutex> lck(mtxUI);	//EnterCriticalSection(&csUI);

    // if TR identification data size is wrong, return NULL
    if (data.size() != STANDARD_IDF_SIZE && data.size() != EXTENDED_IDF_SIZE) return NULL;

	ModuleInfo* modInfo = ant_new ModuleInfo();
    size_t msgBodyPos = 4;

    // read serial number
    modInfo->serialNumber[0] = data.at(msgBodyPos);
	modInfo->serialNumber[1] = data.at(msgBodyPos+1);
	modInfo->serialNumber[2] = data.at(msgBodyPos+2);
    modInfo->serialNumber[3] = data.at(msgBodyPos+3);

    // read OS version
    unsigned int infoId = ModuleInfo::SN_SIZE;
	modInfo->osVersion = data.at(msgBodyPos+infoId);
	infoId++;
    // read TR module type
	modInfo->trType = data.at(msgBodyPos+infoId);
    infoId++;

    // read OS build
	for (unsigned int i = 0; i < ModuleInfo::BUILD_SIZE; i++, infoId++) {
		modInfo->osBuild[i] = data.at(msgBodyPos+infoId);
	}

    // read reserved area
    for (unsigned int i = 0; i < ModuleInfo::RESERVED_SIZE; i++, infoId++) {
		modInfo->reserved[i] = data.at(msgBodyPos+infoId);
	}

    // check if TR module supports extended idf format
    unsigned int extendedIdfFormat = 0;
    if (data.size() == EXTENDED_IDF_SIZE) extendedIdfFormat = 1;

    // read individual bonding key
    for (unsigned int i = 0; i < ModuleInfo::IBK_SIZE; i++, infoId++) {
        if (extendedIdfFormat){
            modInfo->ibk[i] = data.at(msgBodyPos+infoId);
        }
        else {
            modInfo->ibk[i] = 0;
        }
	}

	//LeaveCriticalSection(&csUI);
	return modInfo;
}

SPIStatus CDCMessageParser::getParsedSPIStatus(ustring& data) {
	std::lock_guard<std::mutex> lck(mtxUI);	//EnterCriticalSection(&csUI);

	SPIStatus spiStatus;
    size_t msgBodyPos = 3;

	int parsedValue = data.at(msgBodyPos);
	if (parsedValue < 0) {
		parsedValue += 256;
	}

	if (implObj->spiModes.find((SPIModes)parsedValue) != implObj->spiModes.end()) {
		spiStatus.SPI_MODE = (SPIModes)parsedValue;
		spiStatus.isDataReady = false;
	} else {
		spiStatus.DATA_READY = (SPIModes)parsedValue;
		spiStatus.isDataReady = true;
	}

	//LeaveCriticalSection(&csUI);
	return spiStatus;
}

DSResponse CDCMessageParser::getParsedDSResponse(ustring& data) {
	std::lock_guard<std::mutex> lck(mtxUI);	//EnterCriticalSection(&csUI);

	size_t msgBodyPos = 4;
	size_t bodyLen = data.length() - 1 - msgBodyPos;
	ustring msgBody = data.substr(msgBodyPos, bodyLen);

	if (msgBody == uchar_str("OK")) {
        //LeaveCriticalSection(&csUI);
		return OK;
	}

	if (msgBody == uchar_str("ERR")) {
		//LeaveCriticalSection(&csUI);
		return ERR;
	}

	if (msgBody == uchar_str("BUSY")) {
		//LeaveCriticalSection(&csUI);
		return BUSY;
	}

	//LeaveCriticalSection(&csUI);

	// error - unknown type of reponse
	std::stringstream excStream;
	excStream << "Unknown DS response value: " << msgBody.c_str();
	throw CDCMessageParserException((excStream.str()).c_str());
}

ustring CDCMessageParser::getParsedDRData(ustring& data) {
	std::lock_guard<std::mutex> lck(mtxUI);	//EnterCriticalSection(&csUI);

    size_t userDataStart = 5;
	size_t userDataLen = data.length() - 1 - userDataStart;
	ustring userData = data.substr(5, userDataLen);

	//LeaveCriticalSection(&csUI);
	return userData;
}

PTEResponse CDCMessageParser::getParsedPEResponse(ustring& data) {
	std::lock_guard<std::mutex> lck(mtxUI);	//EnterCriticalSection(&csUI);

	size_t msgBodyPos = 4;
	size_t bodyLen = data.length() - 1 - msgBodyPos;
	ustring msgBody = data.substr(msgBodyPos, bodyLen);

	if (msgBody == uchar_str("OK")) {
        //LeaveCriticalSection(&csUI);
		return PTEResponse::OK;
	}

	if (msgBody == uchar_str("ERR1")) {
		//LeaveCriticalSection(&csUI);
		return PTEResponse::ERR1;
	}

	//LeaveCriticalSection(&csUI);

	// error - unknown type of reponse
	std::stringstream excStream;
	excStream << "Unknown PE response value: " << msgBody.c_str();
	throw CDCMessageParserException((excStream.str()).c_str());
}

PTEResponse CDCMessageParser::getParsedPTResponse(ustring& data) {
	std::lock_guard<std::mutex> lck(mtxUI);	//EnterCriticalSection(&csUI);

	size_t msgBodyPos = 4;
	size_t bodyLen = data.length() - 1 - msgBodyPos;
	ustring msgBody = data.substr(msgBodyPos, bodyLen);

	if (msgBody == uchar_str("OK")) {
        //LeaveCriticalSection(&csUI);
		return PTEResponse::OK;
	}

	if (msgBody == uchar_str("ERR1")) {
		//LeaveCriticalSection(&csUI);
		return PTEResponse::ERR1;
	}

	//LeaveCriticalSection(&csUI);

	// error - unknown type of reponse
	std::stringstream excStream;
	excStream << "Unknown PT response value: " << msgBody.c_str();
	throw CDCMessageParserException((excStream.str()).c_str());
}

PMResponse CDCMessageParser::getParsedPMResponse(ustring& data) {
	std::lock_guard<std::mutex> lck(mtxUI);	//EnterCriticalSection(&csUI);

	size_t msgBodyPos = 4;
	size_t bodyLen = data.length() - 1 - msgBodyPos;
	ustring msgBody = data.substr(msgBodyPos, bodyLen);

	if (msgBody == uchar_str("OK")) {
        //LeaveCriticalSection(&csUI);
		return PMResponse::OK;
	}

	if (msgBody == uchar_str("ERR2")) {
		//LeaveCriticalSection(&csUI);
		return PMResponse::ERR2;
	}

	if (msgBody == uchar_str("ERR3")) {
		//LeaveCriticalSection(&csUI);
		return PMResponse::ERR3;
	}

	if (msgBody == uchar_str("ERR4")) {
		//LeaveCriticalSection(&csUI);
		return PMResponse::ERR4;
	}

	if (msgBody == uchar_str("ERR5")) {
		//LeaveCriticalSection(&csUI);
		return PMResponse::ERR5;
	}

	if (msgBody == uchar_str("ERR6")) {
		//LeaveCriticalSection(&csUI);
		return PMResponse::ERR6;
	}

	if (msgBody == uchar_str("ERR7")) {
		//LeaveCriticalSection(&csUI);
		return PMResponse::ERR7;
	}

	if (msgBody == uchar_str("BUSY")) {
		//LeaveCriticalSection(&csUI);
		return PMResponse::BUSY;
	}

	//LeaveCriticalSection(&csUI);

	// error - unknown type of reponse
	std::stringstream excStream;
	excStream << "Unknown PM response value: " << msgBody.c_str();
	throw CDCMessageParserException((excStream.str()).c_str());
}

ustring CDCMessageParser::getParsedPMData(ustring& data) {
	std::lock_guard<std::mutex> lck(mtxUI);	//EnterCriticalSection(&csUI);

	size_t userDataStart = 4;
	size_t userDataLen = data.length() - 1 - userDataStart;
	ustring userData = data.substr(userDataStart, userDataLen);

	//LeaveCriticalSection(&csUI);
	return userData;
}
