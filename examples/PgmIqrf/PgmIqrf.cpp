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

/**
 * TR module programming example (*.iqrf file)
 *
 * @author      Dusan Machut
 * @version     1.0.0
 * @date        1.5.2018
 */

#include <CDCImpl.h>
#include <iostream>
#include <cstring>
#include <thread>         // std::this_thread::sleep_for

/************************************/
/* Private constants                */
/************************************/
#define IQRF_PGM_FILE_DATA_READY      0
#define IQRF_PGM_FILE_DATA_ERROR      1
#define IQRF_PGM_END_OF_FILE          2

#define TARGET_FLASH_W                0x85
#define TARGET_FLASH_R                0x05
#define TARGET_PLUGIN_W               0x88
#define TARGET_CFG_RFBAND_W           0x82
#define TARGET_CFG_RFBAND_R           0x02
#define TARGET_CFG_RFPGM_W            0x81
#define TARGET_CFG_RFPGM_R            0x01
#define TARGET_CFG_HWP_W              0x80
#define TARGET_CFG_HWP_R              0x00
#define TARGET_CFG_PASSWORD_W         0x83
#define TARGET_CFG_USERKEY_W          0x84
#define TARGET_EEPROM_W               0x86
#define TARGET_EEPROM_R               0x06
#define TARGET_EEEPROM_W              0x87
#define TARGET_EEEPROM_R              0x07

/************************************/
/* Private functions predeclaration */
/************************************/
uint8_t iqrfPgmConvertToNum(uint8_t dataByteHi, uint8_t dataByteLo);
uint8_t iqrfPgmReadIQRFFileLine(void);
void printDataInHex(unsigned char *data, unsigned int length);
char iqrfReadByteFromFile(void);

/************************************/
/* Private variables                */
/************************************/
uint8_t IqrfPgmCodeLineBuffer[64];
FILE *file = NULL;

/**
 * Prints specified data onto standard output in hex format.
 *
 * @param [in,out]  data    Pointer to data buffer.
 * @param           length  The length of the data.
 */
void printDataInHex(unsigned char* data, unsigned int length) {
    for ( int i = 0; i < length; i++ ) {
        std::cout << "0x" << std::hex << (int)*data;
        data++;
        if ( i != (length - 1) ) {
            std::cout << " ";
        }
    }
    std::cout << std::dec << "\n";
}

/**
 * Convetr two ASCII chars tu number
 * @param dataByteHi High nibble in ASCII
 * @param dataByteLo Low nibble in ASCII
 * @return Number
 */
uint8_t iqrfPgmConvertToNum(uint8_t dataByteHi, uint8_t dataByteLo)
{
  uint8_t result = 0;

  /* convert High nibble */
  if (dataByteHi >= '0' && dataByteHi <= '9') {
    result = (dataByteHi - '0') << 4;
  }
  else if (dataByteHi >= 'a' && dataByteHi <= 'f') {
    result = (dataByteHi - 87) << 4;
  }

  /* convert Low nibble */
  if (dataByteLo >= '0' && dataByteLo <= '9') {
    result |= (dataByteLo - '0');
  }
  else if (dataByteLo >= 'a' && dataByteLo <= 'f') {
    result |= (dataByteLo - 87);
  }

  return(result);
}

/**
 * Read one char from input file
 * @return 0 - END OF FILE or char)
 */
char iqrfReadByteFromFile(void)
{
  int ReadChar;

  ReadChar = fgetc( file );

  if (ReadChar == EOF) return 0;

  return ReadChar;
}

/**
 * Read and process line from plugin file
 * @return Return code (IQRF_PGM_FILE_DATA_READY - iqrf file line ready, IQRF_PGM_FILE_DATA_READY - input file format error, IQRF_PGM_END_OF_FILE - end of file)
 */
uint8_t iqrfPgmReadIQRFFileLine(void)
{
    uint8_t FirstChar;
    uint8_t SecondChar;
    uint8_t CodeLineBufferPtr = 0;

repeat_read:
    // read one char from file
    FirstChar = tolower(iqrfReadByteFromFile());

    // read one char from file
    if (FirstChar == '#') {
      // read data to end of line
      while (((FirstChar = iqrfReadByteFromFile()) != 0) && (FirstChar != 0x0D));
    }

    // if end of line
    if (FirstChar == 0x0D) {
      // read second code 0x0A
      iqrfReadByteFromFile();
      if (CodeLineBufferPtr == 0) {
        // read another line
        goto repeat_read;
      }
      if (CodeLineBufferPtr == 20) {
        // line with data readed successfully
        return(IQRF_PGM_FILE_DATA_READY);
      }
      else {
        // wrong file format (error)
        return(IQRF_PGM_FILE_DATA_ERROR);
      }
    }

    // if end of file
    if (FirstChar == 0) {
      return(IQRF_PGM_END_OF_FILE);
    }

    // read second character from code file
    SecondChar = tolower(iqrfReadByteFromFile());
    if (CodeLineBufferPtr >= 20) return(IQRF_PGM_FILE_DATA_ERROR);
    // convert chars to number and store to buffer
    IqrfPgmCodeLineBuffer[CodeLineBufferPtr++] = iqrfPgmConvertToNum(FirstChar, SecondChar);
    // read next data
    goto repeat_read;
}

/**
 * Main entry-point for this application.
 *
 * @return	Exit-code for the process - 0 for success, else an error code.
 */
int main(int argc, char** argv)
{
  int OpResult;

#if defined(WIN32) && defined(_DEBUG)
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
  std::string port_name;
  // check input parameters
  if (argc < 3) {
    std::cerr << "Usage" << std::endl;
    std::cerr << "  PgmIqrfExample <port-name> <file-name>" << std::endl << std::endl;
    std::cerr << "Example" << std::endl;
    std::cerr << "  PgmIqrfExample COM5 test.iqrf" << std::endl;
    std::cerr << "  PgmIqrfExample /dev/ttyACM0 test.iqrf" << std::endl;
    return (-1);
  }
  else {
    port_name = argv[1];

    // We assume argv[2] is a filename to open
    file = fopen( argv[2], "r" );
    // fopen returns 0, the NULL pointer, on failure
    if ( file == 0 ) {
      std::cout << "Could not open input file" << std::endl;
      return (-2);
    }
  }

  CDCImpl* testImp = NULL;
  try {
    // crate cdc implementation object;;
    testImp = ant_new CDCImpl(port_name.c_str());

    // check the connection to GW-USB-xx device
    bool test = testImp->test();
    if ( test ) {
      std::cout << "Connection test OK\n";
    } else {
      std::cout << "Connection test FAILED\n";
      delete testImp;
      fclose(file);
      return 2;
    }
  } catch ( CDCImplException& e ) {
    std::cout << e.getDescr() << "\n";
    if ( testImp != NULL ) {
      delete testImp;
    }
    fclose(file);
    return 1;
  }

  // switch device to programming mode
  try {
    std::cout << "Entering programming mode" << std::endl;
    PTEResponse pteResponse = testImp->enterProgrammingMode();
    if ( pteResponse == PTEResponse::OK ) {
      std::cout << "Programming mode OK" << std::endl;
    }
    else {
      std::cout << "Programming mode ERROR" << std::endl;
      if ( testImp != NULL ) {
        delete testImp;
      }
      fclose(file);
      return 1;
    }
  } catch ( CDCSendException& ex ) {
    std::cout << ex.getDescr() << std::endl;
    // send exception processing...
  } catch ( CDCReceiveException& ex ) {
    std::cout << ex.getDescr() << std::endl;
    // receive exception processing...
  }

  // read data from input file and write it to TR module in GW-USB-xx device
  while ((OpResult = iqrfPgmReadIQRFFileLine()) == IQRF_PGM_FILE_DATA_READY) {
    std::cout << "Data to write:" << std::endl;
    printDataInHex(IqrfPgmCodeLineBuffer, 20);
    std::cout << "Data sent to device" << std::endl;

    // write data to TR module
    try {
      PMResponse pmResponse = testImp->upload(TARGET_PLUGIN_W, IqrfPgmCodeLineBuffer, 20);
      if ( pmResponse == PMResponse::OK ) {
        std::cout << "Data programming OK" << std::endl;
      }
      else {
        std::cout << "Data programming failed" << std::endl;
      }
    } catch ( CDCSendException& ex ) {
      std::cout << ex.getDescr() << std::endl;
      // send exception processing...
    } catch ( CDCReceiveException& ex ) {
      std::cout << ex.getDescr() << std::endl;
      // receive exception processing...
    }
  }

  // switch device to normal mode
  try {
    std::cout << "Terminating programming mode" << std::endl;
    PTEResponse pteResponse = testImp->terminateProgrammingMode();
    if ( pteResponse == PTEResponse::OK ) {
      std::cout << "Programming mode termination OK" << std::endl;
    }
    else {
      std::cout << "Programming mode termination ERROR" << std::endl;
    }
  } catch ( CDCSendException& ex ) {
    std::cout << ex.getDescr() << std::endl;
    // send exception processing...
  } catch ( CDCReceiveException& ex ) {
    std::cout << ex.getDescr() << std::endl;
    // receive exception processing...
  }

  delete testImp;
  fclose(file);
  return 0;
}
