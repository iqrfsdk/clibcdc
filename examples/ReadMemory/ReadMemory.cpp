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
 * TR module memory reading example
 *
 * @author      Dusan Machut
 * @version     1.0.1
 * @date        16.12.2018
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


/************************************/
/* Functions                        */
/************************************/
/**
 * Prints specified data onto standard output in hex format.
 *
 * @param [in,out]  data    Pointer to data buffer.
 * @param           length  The length of the data.
 */
void printDataInHex(unsigned char* data, unsigned int length)
{
    for ( int i = 0; i < length; i++ ) {
        std::cout << "0x" << std::hex << (int)*data;
        data++;
        if ( i != (length - 1) )
            std::cout << " ";
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
    uint8_t   RqBuffer[32];
    uint8_t   RsBuffer[256];
    uint16_t  MemAddress;
    int Cnt, CntEnd;
    int Target;
    unsigned int RsDataLen;

#if defined(WIN32) && defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    std::string port_name;
    // check input parameters
    if (argc < 3) {
        std::cerr << "Usage" << std::endl;
        std::cerr << "  ReadMemoryExample <port-name> <-memory>" << std::endl << std::endl;
        std::cerr << "  memory: -flash or -eeprom or -eeeprom" << std::endl << std::endl;
        std::cerr << "Example" << std::endl;
        std::cerr << "  ReadMemoryExample COM5 -eeprom" << std::endl;
        std::cerr << "  ReadMemoryExample /dev/ttyACM0 -eeprom" << std::endl;
        return (-1);
    } else {
        port_name = argv[1];

        if (strcmp(argv[2], "-eeprom") == 0)
            Target = TARGET_EEPROM_R;
        else if (strcmp(argv[2], "-eeeprom") == 0)
            Target = TARGET_EEEPROM_R;
        else if (strcmp(argv[2], "-flash") == 0)
            Target = TARGET_FLASH_R;
        else {
          std::cerr << "Unsupported memory type" << std::endl;
          return(-2);
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
            return 2;
        }
    } catch ( CDCImplException& e ) {
        std::cout << e.getDescr() << "\n";
        if ( testImp != NULL )
            delete testImp;
        return 1;
    }

    // switch device to programming mode
    try {
        std::cout << "Entering programming mode" << std::endl;
        PTEResponse pteResponse = testImp->enterProgrammingMode();
        if ( pteResponse == PTEResponse::OK ) {
            std::cout << "Programming mode OK" << std::endl;
        } else {
            std::cout << "Programming mode ERROR" << std::endl;
            if ( testImp != NULL )
                delete testImp;
            return 1;
        }
    } catch ( CDCSendException& ex ) {
        std::cout << ex.getDescr() << std::endl;
        // send exception processing...
    } catch ( CDCReceiveException& ex ) {
        std::cout << ex.getDescr() << std::endl;
        // receive exception processing...
    }

    // read selected memory data from TR module in GW-USB-xx device
    if (Target == TARGET_FLASH_R)
        MemAddress = 0x3A00;    // start address for FLASH
    else
        MemAddress = 0x0000;    // start address for internal / external EEPROM

    CntEnd = 8;
    if (Target == TARGET_EEPROM_R)
        CntEnd = 6;

    for (Cnt = 0; Cnt < CntEnd; Cnt++) {
        switch (Target) {
        case TARGET_EEPROM_R:
            printf("Reading 32 bytes of data from internal EEPROM - Address 0x%04x\n\r", MemAddress);
            break;

        case TARGET_EEEPROM_R:
            printf("Reading 32 bytes of data from external EEPROM - Address 0x%04x\n\r", MemAddress);
            break;

        case TARGET_FLASH_R:
            printf("Reading 32 bytes of verify data from FLASH - Address 0x%04x\n\r", MemAddress);
            break;
        }

        RqBuffer[0] = MemAddress & 0xFF;
        RqBuffer[1] = MemAddress >> 8;

        // send request to TR module
        try {
            PMResponse pmResponse = testImp->download(Target, RqBuffer, 2, RsBuffer, sizeof(RsBuffer), RsDataLen);
            if ( pmResponse == PMResponse::OK ) {
                std::cout << "Data reading OK" << std::endl;
                // print readed data
                printDataInHex(RsBuffer, RsDataLen);
            } else {
                std::cout << "Data reading failed" << std::endl;
            }
        } catch ( CDCSendException& ex ) {
            std::cout << ex.getDescr() << std::endl;
            // send exception processing...
        } catch ( CDCReceiveException& ex ) {
            std::cout << ex.getDescr() << std::endl;
            // receive exception processing...
        }

        // new line
        std::cout << std::endl;

        // address for next memory block
        MemAddress += 32;
    }

    // switch device to normal mode
    try {
        std::cout << "Terminating programming mode" << std::endl;
        PTEResponse pteResponse = testImp->terminateProgrammingMode();
        if ( pteResponse == PTEResponse::OK )
            std::cout << "Programming mode termination OK" << std::endl;
        else
            std::cout << "Programming mode termination ERROR" << std::endl;
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
