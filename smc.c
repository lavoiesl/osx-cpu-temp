/*
 * Modified by Sriram Swaminarayan to match
 * output of 'powermetrics' on OSX Big Sur
 * sriram@pobox.com
 * December 02, 2021, or true palindrome day
 */

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
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "smc.h"

static io_connect_t conn;

double valToDouble(SMCVal_t val)
{
    double ret;
    if (val.dataSize > 0) {
        if (!strcmp("int", val.dataType)) {
            ret = (double)*(int*)val.bytes;
        } else if (!strcmp("ui32", val.dataType)) {
            ret = (double)*(uint32_t*)val.bytes;
        } else if (!strcmp("ui16", val.dataType)) {
            ret = (double)*(uint16_t*)val.bytes;
        } else if (!strcmp("ui8 ", val.dataType)) {
            ret = (double)*(uint8_t*)val.bytes;
        } else if (!strcmp("sp78", val.dataType)) {
            // Discarding the exponent bits, not sure if that's OK
            int a = val.bytes[0];
            uint b = *(uint8_t*)(val.bytes + 1);
            int i16 = (((int)a) << 6) + (b >> 2);
            ret = (double)i16;
        } else if (!strcmp("fpe2", val.dataType)) {
            // No sign bit
            int itmp = ((int)val.bytes[0]) * 256 + (unsigned char)(val.bytes[1] >> 2);
            ret = (double)itmp;
        } else if (!strcmp("flt ", val.dataType)) {
            ret = (double)*(float*)val.bytes;
        } else if (!strcmp("dbl ", val.dataType)) {
            ret = (double)*(double*)val.bytes;
        } else {
            printf("__________________unknown SMCVal code=%s %d %d\n", val.dataType, val.bytes[0], val.bytes[1]);
            ret = NAN;
        }
    } else {
        ret = 0.0;
    }
    return ret;
}

int valToInt(SMCVal_t val)
{
    return (int)(valToDouble(val));
}

float valToFloat(SMCVal_t val)
{
    return (float)(valToDouble(val));
}

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

double SMCGetTemperature(char* key)
{
    SMCVal_t val;
    kern_return_t result;

    result = SMCReadKey(key, &val);
    if (result == kIOReturnSuccess) {
        return valToDouble(val) / 64.;
    }
    // read failed
    return 0.0;
}

float SMCGetFanSpeed(char* key)
{
    SMCVal_t val;
    kern_return_t result;

    result = SMCReadKey(key, &val);
    if (result == kIOReturnSuccess) {
        // read succeeded - check returned value
        return valToFloat(val);
    }
    // read failed
    return 0.0;
}

double convertToFahrenheit(double celsius)
{
    return (celsius * (9.0 / 5.0)) + 32.0;
}

// Requires SMCOpen()
void readAndPrintCpuTemp(int show_title, char scale)
{
    double temperature = SMCGetTemperature(SMC_KEY_CPU_TEMP);
    if (scale == 'F') {
        temperature = convertToFahrenheit(temperature);
    }

    char* title = "";
    if (show_title) {
        title = "CPU: ";
    }
    printf("%s%0.1f °%c\n", title, temperature, scale);
}

// Requires SMCOpen()
void readAndPrintGpuTemp(int show_title, char scale)
{
    double temperature = SMCGetTemperature(SMC_KEY_GPU_TEMP);
    if (scale == 'F') {
        temperature = convertToFahrenheit(temperature);
    }

    char* title = "";
    if (show_title) {
        title = "GPU: ";
    }
    printf("%s%0.1f °%c\n", title, temperature, scale);
}

float SMCGetFanRPM(char* key)
{
    SMCVal_t val;
    kern_return_t result;

    result = SMCReadKey(key, &val);
    if (result == kIOReturnSuccess) {
        return valToFloat(val);
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
        totalFans = valToInt(val);
        printf("Num fans: %d\n", totalFans);
        for (i = 0; i < totalFans; i++) {
            sprintf(key, "F%dID", i);
            result = SMCReadKey(key, &val);
            if (result != kIOReturnSuccess) {
                printf("F%d: not found\n", i);
                continue;
            }
            char* name = val.bytes + 4;

            sprintf(key, "F%dAc", i);
            float actual_speed = SMCGetFanSpeed(key); //SS
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
            printf("Fan %d - %s at %.0f RPM (%.0f%%)\n", i, name, actual_speed, pct);

            sprintf(key, "F%dSf", i);
            SMCReadKey(key, &val);
            printf("    Safe speed   : %.0f\n", valToFloat(val));
            sprintf(key, "F%dTg", i);
            SMCReadKey(key, &val);
            printf("    Target speed : %.0f\n", valToFloat(val));
            SMCReadKey("FS! ", &val);
            int mode = valToInt(val);
            if ((mode & (1 << i)) == 0)
                printf("    Mode         : auto\n");
            else
                printf("    Mode         : forced\n");
        }
    }
}

int main(int argc, char* argv[])
{
    char scale = 'C';
    int cpu = 0;
    int fan = 0;
    int gpu = 0;

    int c;
    while ((c = getopt(argc, argv, "CFcfgh?")) != -1) {
        switch (c) {
        case 'F':
        case 'C':
            scale = c;
            break;
        case 'c':
            cpu = 1;
            break;
        case 'f':
            fan = 1;
            break;
        case 'g':
            gpu = 1;
            break;
        case 'h':
        case '?':
            printf("usage: osx-cpu-temp <options>\n");
            printf("Options:\n");
            printf("  -F  Display temperatures in degrees Fahrenheit.\n");
            printf("  -C  Display temperatures in degrees Celsius (Default).\n");
            printf("  -c  Display CPU temperature (Default).\n");
            printf("  -g  Display GPU temperature.\n");
            printf("  -f  Display fan speeds.\n");
            printf("  -h  Display this help.\n");
            printf("\nIf more than one of -c, -f, or -g are specified, titles will be added\n");
            return -1;
        }
    }

    if (!fan && !gpu) {
        cpu = 1;
    }

    int show_title = fan + gpu + cpu > 1;

    SMCOpen();

    if (cpu) {
        readAndPrintCpuTemp(show_title, scale);
    }
    if (gpu) {
        readAndPrintGpuTemp(show_title, scale);
    }
    if (fan) {
        readAndPrintFanRPMs();
    }

    SMCClose();
    return 0;
}
