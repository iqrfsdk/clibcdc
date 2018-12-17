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
 * USB device & TR module identification reading example
 *
 * @author      Dusan Machut
 * @version     1.0.2
 * @date        17.12.2018
 */

#include <CDCImpl.h>
#include <iostream>
#include <cstring>
#include <thread>         // std::this_thread::sleep_for

/************************************/
/* Private functions predeclaration */
/************************************/
void printDataInHex(unsigned char *data, unsigned int length);
void printTrModuleData(ModuleInfo *trModuleInfo);
void printUSBDeviceData(DeviceInfo *usbDeviceInfo);

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
        if (*data < 0x10)
            std::cout << "0x0" << std::hex << (int)*data;
        else
            std::cout << "0x" << std::hex << (int)*data;

        data++;
        if ( i != (length - 1) )
            std::cout << " ";
    }
    std::cout << std::dec << std::endl;
}

/**
 * Decode and print TR module identification data
 *
 * @param [in]	trModuleInfo	Pointer to structure with TR module identification data
 */
void printTrModuleData(ModuleInfo *trModuleInfo)
{
    // MCU type of TR module
    #define MCU_UNKNOWN                   0
    #define PIC16LF819                    1     // TR-xxx-11A not supported
    #define PIC16LF88                     2     // TR-xxx-21A
    #define PIC16F886                     3     // TR-31B, TR-52B, TR-53B
    #define PIC16LF1938                   4     // TR-52D, TR-54D

    // TR module types
    #define TR_52D                        0
    #define TR_58D_RJ                     1
    #define TR_72D                        2
    #define TR_53D                        3
    #define TR_54D                        8
    #define TR_55D                        9
    #define TR_56D                        10
    #define TR_76D                        11

    // FCC cerificate
    #define FCC_NOT_CERTIFIED             0
    #define FCC_CERTIFIED                 1

    uint32_t moduleId;
    uint16_t osBuild;
    uint8_t osVersionMajor, osVersionMinor;
    uint8_t moduleType;
    uint8_t mcuType;
    uint8_t fccCerificate;
    uint8_t tempStringPtr = 0;
    char tempString[64];

    std::cout << "TR module info data:" << std::endl;
    std::cout << "--------------------" << std::endl;

    // decode identification data
    moduleId = (uint32_t)trModuleInfo->serialNumber[3] << 24 |
               (uint32_t)trModuleInfo->serialNumber[2] << 16 |
               (uint32_t)trModuleInfo->serialNumber[1] << 8 |
               (uint32_t)trModuleInfo->serialNumber[0];

    moduleType = trModuleInfo->trType >> 4;
    mcuType = trModuleInfo->trType & 0x07;
    fccCerificate = (trModuleInfo->trType & 0x08) >> 3;
    osVersionMajor = trModuleInfo->osVersion / 16;
    osVersionMinor = trModuleInfo->osVersion % 16;
    osBuild = (uint16_t)trModuleInfo->osBuild[1] << 8 | trModuleInfo->osBuild[0];

    // print module Type
    if (moduleId & 0x80000000L) {
        tempString[tempStringPtr++] = 'D';
        tempString[tempStringPtr++] = 'C';
    }
    tempString[tempStringPtr++] = 'T';
    tempString[tempStringPtr++] = 'R';
    tempString[tempStringPtr++] = '-';
    tempString[tempStringPtr++] = '5';

    switch(moduleType) {
    case TR_52D:
        tempString[tempStringPtr++] = '2';
        break;
    case TR_58D_RJ:
        tempString[tempStringPtr++] = '8';
        break;
    case TR_72D:
        tempString[tempStringPtr-1] = '7';
        tempString[tempStringPtr++] = '2';
        break;
    case TR_53D:
        tempString[tempStringPtr++] = '3';
        break;
    case TR_54D:
        tempString[tempStringPtr++] = '4';
        break;
    case TR_55D:
        tempString[tempStringPtr++] = '5';
        break;
    case TR_56D:
        tempString[tempStringPtr++] = '6';
        break;
    case TR_76D:
        tempString[tempStringPtr-1] = '7';
        tempString[tempStringPtr++] = '6';
        break;
    default :
        tempString[tempStringPtr++] = 'x';
        break;
    }

    if(mcuType == PIC16LF1938)
        tempString[tempStringPtr++]='D';
    tempString[tempStringPtr++]='x';
    tempString[tempStringPtr++] = 0;
    std::cout << "Module type:       " << tempString << std::endl;

    // print module MCU
    std::cout << "Module MCU:        ";
    switch (mcuType) {
    case PIC16LF819:
        std::cout << "PIC16LF819" << std::endl;
        break;
    case PIC16LF88:
        std::cout << "PIC16LF88" << std::endl;
        break;
    case PIC16F886:
        std::cout << "PIC16F886" << std::endl;
        break;
    case PIC16LF1938:
        std::cout << "PIC16LF1938" << std::endl;
        break;
    default:
        std::cout << "UNKNOWN" << std::endl;
        break;
    }

    // print module MCU
    sprintf(tempString, "Module ID:         0x%.8X", moduleId);
    std::cout << tempString << std::endl;

    // print OS version & build
    sprintf(tempString, "OS version:       %2X.%02XD (0x%04x)", osVersionMajor, osVersionMinor, osBuild);
    std::cout << tempString << std::endl;

    // print module FCC certification
    sprintf(tempString, "FCC certification: ");
    if (fccCerificate == FCC_CERTIFIED)
        strcat(tempString, "YES");
    else
        strcat(tempString, "NO");
    std::cout << tempString << std::endl;

    // of OS version is 4.03 and more, print IBK
    std::cout << "IBK:               ";
    if ((osVersionMajor > 4) || (osVersionMajor == 4 && osVersionMinor >= 3))
        printDataInHex((uint8_t *)&trModuleInfo->ibk[0], 16);
    else
        std::cout << "---";

    std::cout << std::endl;
}

/**
 * print USB device identification data
 *
 * @param [in]	getUSBDeviceInfo	Pointer to structure with gateway identification data
 */
void printUSBDeviceData(DeviceInfo *usbDeviceInfo)
{
    char tempString[64];

    std::cout << "USB device info data:" << std::endl;
    std::cout << "---------------------" << std::endl;

    // print USB device type
    if (usbDeviceInfo->typeLen >= sizeof(tempString))
        usbDeviceInfo->typeLen = sizeof(tempString) - 1;
    memcpy(tempString, usbDeviceInfo->type, usbDeviceInfo->typeLen);
    tempString[usbDeviceInfo->typeLen] = 0;
    std::cout << "USB device type:     " << tempString << std::endl;

    // print USB device FW version
    if (usbDeviceInfo->fwLen >= sizeof(tempString))
        usbDeviceInfo->fwLen = sizeof(tempString) - 1;
    memcpy(tempString, usbDeviceInfo->firmwareVersion, usbDeviceInfo->fwLen);
    tempString[usbDeviceInfo->fwLen] = 0;
    std::cout << "USB device firmware: " << tempString << std::endl;

    // print USB device serial number
    if (usbDeviceInfo->snLen >= sizeof(tempString))
        usbDeviceInfo->snLen = sizeof(tempString) - 1;
    memcpy(tempString, usbDeviceInfo->serialNumber, usbDeviceInfo->snLen);
    tempString[usbDeviceInfo->snLen] = 0;
    std::cout << "USB device SN:       " << tempString << std::endl;

    std::cout << std::endl;
}

/**
 * Main entry-point for this application.
 *
 * @return	Exit-code for the process - 0 for success, else an error code.
 */
int main(int argc, char** argv)
{
    std::string port_name;
    // check input parameters
    if (argc < 2) {
        std::cerr << "Usage" << std::endl;
        std::cerr << "  ReadTrIdfExample <port-name>" << std::endl << std::endl;
        std::cerr << "Example" << std::endl;
        std::cerr << "  ReadTrIdfExample COM5" << std::endl;
        std::cerr << "  ReadTrIdfExample /dev/ttyACM0" << std::endl;
        return (-1);
    } else {
        port_name = argv[1];
    }

    CDCImpl* testImp = NULL;
    try {
        // crate cdc implementation object;;
        testImp = ant_new CDCImpl(port_name.c_str());

        // check the connection to GW-USB-xx device
        bool test = testImp->test();
        if ( test ) {
            std::cout << "Connection test OK" << std::endl;
        } else {
            std::cout << "Connection test FAILED" << std::endl;
            delete testImp;
            return 2;
        }
    } catch ( CDCImplException& e ) {
        std::cout << e.getDescr() << std::endl;
        if ( testImp != NULL )
            delete testImp;
        return 1;
    }
    std::cout << std::endl;

    // read identification data from gateway
    try {
        DeviceInfo *MyDeviceInfoData = testImp->getUSBDeviceInfo();
        if ( MyDeviceInfoData != NULL ) {
            std::cout << "USB device reading OK" << std::endl;
            // print Identification data
            printUSBDeviceData(MyDeviceInfoData);
        } else {
            std::cout << "USB device reading failed" << std::endl;
        }
    } catch ( CDCSendException& ex ) {
        std::cout << ex.getDescr() << std::endl;
        // send exception processing...
    } catch ( CDCReceiveException& ex ) {
        std::cout << ex.getDescr() << std::endl;
        // receive exception processing...
    }

    // read identification data from TR module
    try {
        ModuleInfo *MyModuleInfoData = testImp->getTRModuleInfo();
        if ( MyModuleInfoData != NULL ) {
            std::cout << "TR module reading OK" << std::endl;
            // print Identification data
            printTrModuleData(MyModuleInfoData);
        } else {
            std::cout << "TR module reading failed" << std::endl;
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
