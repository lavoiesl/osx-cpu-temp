/*
 * Apple System Management Control (SMC) Tool 
 * Copyright (C) 2006 devnull 
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <IOKit/IOKitLib.h>

#include "smc.h"

static io_connect_t conn;

UInt32 _strtoul(char *str, int size, int base)
{
    UInt32 total = 0;
    int i;

    for (i = 0; i < size; i++)
    {
        if (base == 16)
            total += str[i] << (size - 1 - i) * 8;
        else
           total += (unsigned char) (str[i] << (size - 1 - i) * 8);
    }
    return total;
}

void _ultostr(char *str, UInt32 val)
{
    str[0] = '\0';
    sprintf(str, "%c%c%c%c", 
            (unsigned int) val >> 24,
            (unsigned int) val >> 16,
            (unsigned int) val >> 8,
            (unsigned int) val);
}

kern_return_t SMCOpen(void)
{
    kern_return_t result;
    io_iterator_t iterator;
    io_object_t   device;

    CFMutableDictionaryRef matchingDictionary = IOServiceMatching("AppleSMC");
    result = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDictionary, &iterator);
    if (result != kIOReturnSuccess)
    {
        printf("Error: IOServiceGetMatchingServices() = %08x\n", result);
        return 1;
    }

    device = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (device == 0)
    {
        printf("Error: no SMC found\n");
        return 1;
    }

    result = IOServiceOpen(device, mach_task_self(), 0, &conn);
    IOObjectRelease(device);
    if (result != kIOReturnSuccess)
    {
        printf("Error: IOServiceOpen() = %08x\n", result);
        return 1;
    }

    return kIOReturnSuccess;
}

kern_return_t SMCClose()
{
    return IOServiceClose(conn);
}


kern_return_t SMCCall(int index, SMCKeyData_t *inputStructure, SMCKeyData_t *outputStructure)
{
    size_t   structureInputSize;
    size_t   structureOutputSize;

    structureInputSize = sizeof(SMCKeyData_t);
    structureOutputSize = sizeof(SMCKeyData_t);

    #if MAC_OS_X_VERSION_10_5
    return IOConnectCallStructMethod( conn, index,
                            // inputStructure
                            inputStructure, structureInputSize,
                            // ouputStructure
                            outputStructure, &structureOutputSize );
    #else
    return IOConnectMethodStructureIStructureO( conn, index,
                                                structureInputSize, /* structureInputSize */
                                                &structureOutputSize,   /* structureOutputSize */
                                                inputStructure,        /* inputStructure */
                                                outputStructure);       /* ouputStructure */
    #endif

}

kern_return_t SMCReadKey(UInt32Char_t key, SMCVal_t *val)
{
    kern_return_t result;
    SMCKeyData_t  inputStructure;
    SMCKeyData_t  outputStructure;

    memset(&inputStructure, 0, sizeof(SMCKeyData_t));
    memset(&outputStructure, 0, sizeof(SMCKeyData_t));
    memset(val, 0, sizeof(SMCVal_t));

    inputStructure.key = _strtoul(key, 4, 16);
    inputStructure.data8 = SMC_CMD_READ_KEYINFO;

    result = SMCCall(KERNEL_INDEX_SMC, &inputStructure, &outputStructure);
    if (result != kIOReturnSuccess)
        return result;

    val->dataSize = outputStructure.keyInfo.dataSize;
    _ultostr(val->dataType, outputStructure.keyInfo.dataType);
    inputStructure.keyInfo.dataSize = val->dataSize;
    inputStructure.data8 = SMC_CMD_READ_BYTES;

    result = SMCCall(KERNEL_INDEX_SMC, &inputStructure, &outputStructure);
    if (result != kIOReturnSuccess)
        return result;

    memcpy(val->bytes, outputStructure.bytes, sizeof(outputStructure.bytes));

    return kIOReturnSuccess;
}

double SMCGetTemperature(char *key)
{
    SMCVal_t val;
    kern_return_t result;

    result = SMCReadKey(key, &val);
    if (result == kIOReturnSuccess) {
        // read succeeded - check returned value
        if (val.dataSize > 0) {
            if (strcmp(val.dataType, DATATYPE_SP78) == 0) {
                // convert sp78 value to temperature
                int intValue = val.bytes[0] * 256 + (unsigned char)val.bytes[1];
                return intValue / 256.0;
            }
        }
    }
    // read failed
    return 0.0;
}

unsigned int SMCGetFanRPM(char *key)
{
    SMCVal_t val;
    kern_return_t result;
    
    result = SMCReadKey(key, &val);
    if (result == kIOReturnSuccess) {
        // read succeeded - check returned value
        if (val.dataSize > 0) {
            if (strcmp(val.dataType, DATATYPE_FPE2) == 0) {
                // convert fpe2 value to RPM
                float value = ntohs(*(UInt16*)val.bytes) / 4.0;
//                [0] * 256 + (unsigned char)val.bytes[1];
                return (int)(value);
            }
        }
    }
    // read failed
    return 0.0;
}

double convertToFahrenheit(double celsius) {
  return (celsius * (9.0 / 5.0)) + 32.0;
}

void ReadAndPrintFanRPMs(void)
{
    kern_return_t result;
    SMCVal_t val;
    UInt32Char_t key;
    int totalFans, i;
    
    result = SMCReadKey("FNum", &val);
    
    if(result == kIOReturnSuccess)
    {
        totalFans = _strtoul((char *)val.bytes, val.dataSize, 10);
        
        printf("Num fans: %d\n", totalFans);
        for(i=0; i<totalFans; i++)
        {
            printf(" - Fan %d: ", i);
            sprintf(key, "F%dID", i);
            SMCReadKey(key, &val);
            printf("%s", val.bytes+4);
            sprintf(key, "F%dAc", i);
            SMCReadKey(key, &val);
            //printf("    Actual speed : %.0f\n", _strtof(val.bytes, val.dataSize, 2));
            float actual_speed = (float)(ntohs(*(UInt16*)val.bytes)>>2);
            sprintf(key, "F%dMn", i);
            SMCReadKey(key, &val);
            //printf("    Minimum speed: %.0f\n", strtof(val.bytes, val.dataSize, 2));
            float minimum_speed =(float)(ntohs(*(UInt16*)val.bytes)>>2);
            sprintf(key, "F%dMx", i);
            SMCReadKey(key, &val);
            //printf("    Maximum speed: %.0f\n", strtof(val.bytes, val.dataSize, 2));
            float maximum_speed =(float)(ntohs(*(UInt16*)val.bytes)>>2);
            float rpm = actual_speed - minimum_speed;
            if (rpm < 0.f) rpm = 0.f;
            float pct = (rpm)/(maximum_speed-minimum_speed);
            
            pct *= 100.f;
            printf("at %.0f RPM (%.0f%%)\n", rpm, pct );
            
            //sprintf(key, "F%dSf", i);
            //SMCReadKey(key, &val);
            //printf("    Safe speed   : %.0f\n", strtof(val.bytes, val.dataSize, 2));
            //sprintf(key, "F%dTg", i);
            //SMCReadKey(key, &val);
            //printf("    Target speed : %.0f\n", strtof(val.bytes, val.dataSize, 2));
            //SMCReadKey("FS! ", &val);
            //if ((_strtoul((char *)val.bytes, 2, 16) & (1 << i)) == 0)
            //    printf("    Mode         : auto\n");
            //else
            //    printf("    Mode         : forced\n");
        }
    }
    
    return;
}
    
int main(int argc, char *argv[])
{
    char scale = 'C';
    bool do_gpu = true, do_fan_rpm = true;

    int c;
    while ((c = getopt(argc, argv, "CF")) != -1) {
      switch (c) {
        case 'F':
        case 'C':
          scale = c;
          break;
      }
    }

    SMCOpen();
    double temperature = SMCGetTemperature(SMC_KEY_CPU_TEMP);

    if (scale == 'F') {
      temperature = convertToFahrenheit(temperature);
    }

    printf("CPU: %0.1f°%c\n", temperature, scale);
    
    if( do_gpu ) {
        double gpu_temp = SMCGetTemperature(SMC_KEY_GFX_TEMP);
        
        if(scale == 'F') {
            temperature = convertToFahrenheit(temperature);
        }
        
        printf("GPU: %0.1f°%c\n", gpu_temp, scale);
    }
    
    if( do_fan_rpm) {
        ReadAndPrintFanRPMs();
    }

    SMCClose();

    return 0;
}
