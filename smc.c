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

#include <IOKit/IOKitLib.h>
#include <stdio.h>
#include <string.h>

#include "smc.h"

static io_connect_t conn;

UInt32 _strtoul(char* str, int size, int base)
{
    UInt32 total = 0;
    int i;

    for (i = 0; i < size; i++) {
        if (base == 16)
            total += str[i] << (size - 1 - i) * 8;
        else
            total += (unsigned char)(str[i] << (size - 1 - i) * 8);
    }
    return total;
}

void _ultostr(char* str, UInt32 val)
{
    str[0] = '\0';
    sprintf(str, "%c%c%c%c",
        (unsigned int)val >> 24,
        (unsigned int)val >> 16,
        (unsigned int)val >> 8,
        (unsigned int)val);
}

kern_return_t SMCOpen(void)
{
    kern_return_t result;
    io_iterator_t iterator;
    io_object_t device;

    CFMutableDictionaryRef matchingDictionary = IOServiceMatching("AppleSMC");
    result = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDictionary, &iterator);
    if (result != kIOReturnSuccess) {
        printf("Error: IOServiceGetMatchingServices() = %08x\n", result);
        return 1;
    }

    device = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (device == 0) {
        printf("Error: no SMC found\n");
        return 1;
    }

    result = IOServiceOpen(device, mach_task_self(), 0, &conn);
    IOObjectRelease(device);
    if (result != kIOReturnSuccess) {
        printf("Error: IOServiceOpen() = %08x\n", result);
        return 1;
    }

    return kIOReturnSuccess;
}

kern_return_t SMCClose()
{
    return IOServiceClose(conn);
}

kern_return_t SMCCall(int index, SMCKeyData_t* inputStructure, SMCKeyData_t* outputStructure)
{
    size_t structureInputSize;
    size_t structureOutputSize;

    structureInputSize = sizeof(SMCKeyData_t);
    structureOutputSize = sizeof(SMCKeyData_t);

#if MAC_OS_X_VERSION_10_5
    return IOConnectCallStructMethod(conn, index,
        // inputStructure
        inputStructure, structureInputSize,
        // ouputStructure
        outputStructure, &structureOutputSize);
#else
    return IOConnectMethodStructureIStructureO(conn, index,
        structureInputSize, /* structureInputSize */
        &structureOutputSize, /* structureOutputSize */
        inputStructure, /* inputStructure */
        outputStructure); /* ouputStructure */
#endif
}

kern_return_t SMCReadKey(UInt32Char_t key, SMCVal_t* val)
{
    kern_return_t result;
    SMCKeyData_t inputStructure;
    SMCKeyData_t outputStructure;

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

// Requires SMCOpen()
double SMCGetTemperature(char* key)
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

// Requires SMCOpen()
double SMCGetFanSpeed(char* key)
{
    SMCVal_t val;
    kern_return_t result;

    result = SMCReadKey(key, &val);
    if (result == kIOReturnSuccess) {
        // read succeeded - check returned value
        if (val.dataSize > 0) {
            if (strcmp(val.dataType, DATATYPE_FPE2) == 0) {
                // convert fpe2 value to rpm
                int intValue = (unsigned char)val.bytes[0] * 256 + (unsigned char)val.bytes[1];
                return intValue / 4.0;
            }
        }
    }
    // read failed
    return 0.0;
}

double convertToFahrenheit(double celsius)
{
    return (celsius * (9.0 / 5.0)) + 32.0;
}

double readTemperature(char* key, char scale)
{
    double temperature = SMCGetTemperature(key);
    if (scale == 'F') {
        temperature = convertToFahrenheit(temperature);
    }
    return temperature;
}

void readAndPrintTemperature(char* title, bool show_title, char* key, char scale, bool show_scale)
{
    if (!show_title) {
        title = "";
    }

    double temperature = readTemperature(key, scale);

    if (show_scale) {
        printf("%s%0.1f °%c\n", title, temperature, scale);
    } else {
        printf("%s%0.1f\n", title, temperature);
    }
}

float SMCGetFanRPM(char* key)
{
    SMCVal_t val;
    kern_return_t result;

    result = SMCReadKey(key, &val);
    if (result == kIOReturnSuccess) {
        // read succeeded - check returned value
        if (val.dataSize > 0) {
            if (strcmp(val.dataType, DATATYPE_FPE2) == 0) {
                // convert fpe2 value to RPM
                return ntohs(*(UInt16*)val.bytes) / 4.0;
            }
        }
    }
    // read failed
    return -1.f;
}

// Requires SMCOpen()
void readAndPrintFanRPMs(void)
{
    kern_return_t result;
    SMCVal_t val;
    UInt32Char_t key;
    int totalFans, i;

    result = SMCReadKey("FNum", &val);

    if (result == kIOReturnSuccess) {
        totalFans = _strtoul((char*)val.bytes, val.dataSize, 10);

        printf("Num fans: %d\n", totalFans);
        for (i = 0; i < totalFans; i++) {
            sprintf(key, "F%dID", i);
            result = SMCReadKey(key, &val);
            if (result != kIOReturnSuccess) {
                continue;
            }
            char* name = val.bytes + 4;

            sprintf(key, "F%dAc", i);
            float actual_speed = SMCGetFanRPM(key);
            if (actual_speed < 0.f) {
                continue;
            }

            sprintf(key, "F%dMn", i);
            float minimum_speed = SMCGetFanRPM(key);
            if (minimum_speed < 0.f) {
                continue;
            }

            sprintf(key, "F%dMx", i);
            float maximum_speed = SMCGetFanRPM(key);
            if (maximum_speed < 0.f) {
                continue;
            }

            float rpm = actual_speed - minimum_speed;
            if (rpm < 0.f) {
                rpm = 0.f;
            }
            float pct = rpm / (maximum_speed - minimum_speed);

            pct *= 100.f;
            printf("Fan %d - %s at %.0f RPM (%.0f%%)\n", i, name, rpm, pct);

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
}

int main(int argc, char* argv[])
{
    char scale = 'C';
    bool show_scale = true;
    bool cpu = false;
    bool fan = false;
    bool gpu = false;
    bool amb = false;

    int c;
    while ((c = getopt(argc, argv, "CFTcfgah?")) != -1) {
        switch (c) {
        case 'F':
        case 'C':
            scale = c;
            break;
        case 'T':
            show_scale = false;
            break;
        case 'c':
            cpu = true;
            break;
        case 'f':
            fan = true;
            break;
        case 'g':
            gpu = true;
            break;
        case 'a':
            amb = true;
            break;
        case 'h':
        case '?':
            printf("usage: osx-cpu-temp <options>\n");
            printf("Options:\n");
            printf("  -F  Display temperatures in degrees Fahrenheit.\n");
            printf("  -C  Display temperatures in degrees Celsius (Default).\n");
            printf("  -T  Do not display the units for temperatures.\n");
            printf("  -c  Display CPU temperature (Default).\n");
            printf("  -g  Display GPU temperature.\n");
            printf("  -a  Display ambient temperature.\n");
            printf("  -f  Display fan speeds.\n");
            printf("  -h  Display this help.\n");
            printf("\nIf more than one of -c, -f, or -g are specified, titles will be added\n");
            return -1;
        }
    }

    if (!fan && !gpu && !amb) {
        cpu = true;
    }

    bool show_title = fan + gpu + cpu + amb > 1;

    SMCOpen();

    if (cpu) {
        readAndPrintTemperature("CPU: ", show_title, SMC_KEY_CPU_TEMP, scale, show_scale);
    }
    if (gpu) {
        readAndPrintTemperature("GPU: ", show_title, SMC_KEY_GPU_TEMP, scale, show_scale);
    }
    if (amb) {
        readAndPrintTemperature("Ambient: ", show_title, SMC_KEY_AMBIENT_TEMP, scale, show_scale);
    }
    if (fan) {
        readAndPrintFanRPMs();
    }

    SMCClose();
    return 0;
}
