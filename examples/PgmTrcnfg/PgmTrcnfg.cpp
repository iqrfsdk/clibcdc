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
 * TR module programming example (*.trcnfg file)
 *
 * @author      Dusan Machut
 * @version     1.0.0
 * @date        6.5.2018
 */

#include <CDCImpl.h>
#include <iostream>
#include <cstring>
#include <thread>         // std::this_thread::sleep_for

/************************************/
/* Private constants                */
/************************************/
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
void printDataInHex(unsigned char *data, unsigned int length);

/************************************/
/* Private variables                */
/************************************/
uint8_t HwProfile[32];
uint8_t RfPgmCfg;
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
 * Main entry-point for this application.
 *
 * @return	Exit-code for the process - 0 for success, else an error code.
 */
int main(int argc, char** argv)
{

  PMResponse pmResponse;
  uint32_t CfgFileSize;
  int Cnt;

#if defined(WIN32) && defined(_DEBUG)
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
  std::string port_name;
  // check input parameters
  if (argc < 3) {
    std::cerr << "Usage" << std::endl;
    std::cerr << "  PgmIqrfExample <port-name> <file-name>" << std::endl << std::endl;
    std::cerr << "Example" << std::endl;
    std::cerr << "  PgmIqrfExample COM5 test.trcnfg" << std::endl;
    std::cerr << "  PgmIqrfExample /dev/ttyACM0 test.trcnfg" << std::endl;
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

    // check configuration file size
    fseek(file, 0, SEEK_END);
    CfgFileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (CfgFileSize < 33) {
      std::cout << "Wrong format of *.trcnfg file" << std::endl;
    }
    else {
      // prepare configuration data
      for(Cnt=0; Cnt<32; Cnt++){
        HwProfile[Cnt] = fgetc(file);
      }
      RfPgmCfg = fgetc(file);
    }
    fclose(file);
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
      return 2;
    }
  } catch ( CDCImplException& e ) {
    std::cout << e.getDescr() << "\n";
    if ( testImp != NULL ) {
      delete testImp;
    }
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
      return 1;
    }
  } catch ( CDCSendException& ex ) {
    std::cout << ex.getDescr() << std::endl;
    // send exception processing...
  } catch ( CDCReceiveException& ex ) {
    std::cout << ex.getDescr() << std::endl;
    // receive exception processing...
  }

  // write configuration data to TR module in GW-USB-xx device
  for (Cnt=0; Cnt<2; Cnt++){
    if (Cnt == 0){
      std::cout << "HW profile data to write:" << std::endl;
      printDataInHex(HwProfile, 32);
    }
    else {
      std::cout << "RFPGM data to write:" << std::endl;
      printDataInHex(&RfPgmCfg, 1);
    }
    std::cout << "Data sent to device" << std::endl;

    // write data to TR module
    try {
      if (Cnt == 0){
        pmResponse = testImp->upload(TARGET_CFG_HWP_W, HwProfile, 32);
      }
      else {
        pmResponse = testImp->upload(TARGET_CFG_RFPGM_W, &RfPgmCfg, 1);
      }

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
  return 0;
}
