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

/**
 * Incoming messages parser for CDCImpl class.
 *
 * @author		Michal Konopa
 * @file		CDCMessageParser.h
 * @version		1.0.1
 * @date		12.9.2012
 */

#ifndef __CDCMessageParser_h_
#define __CDCMessageParser_h_

#include <CdcInterface.h>
#include <CDCTypes.h>


/**
 * Result type of parsing process.
 */
enum ParseResultType {
	PARSE_OK,               /**< parsing OK */
	PARSE_NOT_COMPLETE,     /**< not enough data to recognize message format */
	PARSE_BAD_FORMAT        /**< bad format of message */
};

/**
 * Result of incoming data parsing process.
 */
struct ParseResult {
	MessageType msgType;    		/**< standard message type */
	ParseResultType resultType;		/**< result type of parsing */
	unsigned int lastPosition;		/**< last parsed position */
};

/**
 * Forward declaration of CDCMessageParser implementation class.
 */
class CDCMessageParserPrivate;

/**
 * Parser of messages, which come from COM-port. Parser is based on finite
 * automata theory.
 */
class CDCMessageParser {
private:
	// Pointer to implementation object(d-pointer).
	CDCMessageParserPrivate* implObj;

public:
	/**
	 * Constructs message parser.
	 */
	CDCMessageParser();

	/**
	 *  Frees up used resources.
	 */
    ~CDCMessageParser();

	/**
	 * Parses specified data and returns result.
	 * @return result of parsing of specified data
	 */
	ParseResult parseData(ustring& data);

	/**
	 * Returns USB device info from specified data.
	 * @return USB device info from specified data.
	 */
	DeviceInfo* getParsedDeviceInfo(ustring& data);

	/**
	 * Returns TR module info from specified data.
	 * @return TR module info from specified data.
	 */
	ModuleInfo* getParsedModuleInfo(ustring& data);

	/**
	 * Returns SPI status from specified data.
	 * @return SPI status from specified data.
	 */
	SPIStatus getParsedSPIStatus(ustring& data);

	/**
	 * Returns data send response from specified data.
	 * @return data send response from specified data.
	 */
	DSResponse getParsedDSResponse(ustring& data);

	/**
	 * Returns data part of last parsed DR message.
	 * @returns data part of last parsed DR message.
	 */
    ustring getParsedDRData(ustring& data);

	/**
	 * Returns enable programming mode response from specified data.
	 * @return data send response from specified data.
	 */
	PTEResponse getParsedPEResponse(ustring& data);

	/**
	 * Returns terminate programming mode response from specified data.
	 * @return data send response from specified data.
	 */
	PTEResponse getParsedPTResponse(ustring& data);

	/**
	 * Returns upload TR module memory response from specified data.
	 * @return data send response from specified data.
	 */
	PMResponse getParsedPMResponse(ustring& data);

	/**
	 * Returns data part of last parsed PM message.
         * Note: Previous call to parseData() must return MSG_DOWNLOAD_DATA
         * before you run this method. Do not run this method if the call
         * returned MSG_UPLOAD_DOWNLOAD.
	 * @returns data part of last parsed PM message.
	 */
	ustring getParsedPMData(ustring& data);
};

#endif // __CDCMessageParser_h_
