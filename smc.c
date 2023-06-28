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
            total += (str[i] << (size - 1 - i) * 8);
        else
            total += ((unsigned char)(str[i]) << (size - 1 - i) * 8);
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
    result = IOServiceGetMatchingServices(kIOMainPortDefault, matchingDictionary, &iterator);
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

UInt32 SMCReadIndexCount(void)
{
    kern_return_t result;
    SMCVal_t val;

    SMCReadKey("#KEY", &val);
    return _strtoul((char *)val.bytes, val.dataSize, 10);
}

double SMCNormalizeFloat(SMCVal_t val)
{
    if (strcmp(val.dataType, DATATYPE_SP78) == 0) {
        return ((SInt16)ntohs(*(UInt16*)val.bytes)) / 256.0;
    }
    if (strcmp(val.dataType, DATATYPE_SP5A) == 0) {
        return ((SInt16)ntohs(*(UInt16*)val.bytes)) / 1024.0;
    }
    if (strcmp(val.dataType, DATATYPE_FPE2) == 0) {
        return ntohs(*(UInt16*)val.bytes) / 4.0;
    }
    if (strcmp(val.dataType, DATATYPE_FP88) == 0) {
        return ntohs(*(UInt16*)val.bytes) / 256.0;
    }
    return -1.f;
}

int SMCNormalizeInt(SMCVal_t val)
{
    if (strcmp(val.dataType, DATATYPE_UINT8) == 0 || strcmp(val.dataType, DATATYPE_UINT16) == 0 || strcmp(val.dataType, DATATYPE_UINT32) == 0) {
        return (int) _strtoul((char *)val.bytes, val.dataSize, 10);
    }
    if (strcmp(val.dataType, DATATYPE_SI16) == 0) {
        return ntohs(*(SInt16*)val.bytes);
    }

    if (strcmp(val.dataType, DATATYPE_HEX) == 0 || strcmp(val.dataType, DATATYPE_FLAG) == 0) {
        printf("Hex value = 0x");
        int i;
        for (i = 0; i < val.dataSize; i++)
          printf("%02x ", (unsigned char) val.bytes[i]);
        printf("\n");
        return (int) _strtoul((char *)val.bytes, val.dataSize, 10);
    }
    return -1;
}

char* SMCNormalizeText(SMCVal_t val)
{
    char result[val.dataSize + 2];
    if (strcmp(val.dataType, DATATYPE_CH8) == 0) {
        int i;
        for (i = 0; i < val.dataSize; i++) {
            result[i] = (unsigned char)val.bytes[i];
        }
        result[i+1] = 0;
        return strdup(result);
    }

    // convert anything else to text
    double f = SMCNormalizeFloat(val);
    if (f != -1.0) {
        snprintf(result, 10, "%0.1f", f);
        return strdup(result);
    }
    int i = SMCNormalizeInt(val);
    if (i != -1) {
        snprintf(result, 10, "%d", i);
        return strdup(result);
    }

    return strdup(result);
}

kern_return_t SMCPrintAll(void)
{
    kern_return_t result;
    SMCKeyData_t  inputStructure;
    SMCKeyData_t  outputStructure;

    int           totalKeys, i;
    UInt32Char_t  key;
    SMCVal_t      val;

    totalKeys = SMCReadIndexCount();
    printf("Total keys = %d\n", totalKeys);

    for (i = 0; i < totalKeys; i++)
    {
        memset(&inputStructure, 0, sizeof(SMCKeyData_t));
        memset(&outputStructure, 0, sizeof(SMCKeyData_t));
        memset(&val, 0, sizeof(SMCVal_t));

        inputStructure.data8 = SMC_CMD_READ_INDEX;
        inputStructure.data32 = i;

        result = SMCCall(KERNEL_INDEX_SMC, &inputStructure, &outputStructure);
        if (result != kIOReturnSuccess)
            continue;

        _ultostr(key, outputStructure.key);

		SMCReadKey(key, &val);
		char* txt = SMCNormalizeText(val);
		printf("key = %s type = %s value = %s\n", key, val.dataType, txt);
    }

    return kIOReturnSuccess;
}

// Requires SMCOpen()
double SMCGetTemperature(char* key)
{
    kern_return_t result;
    SMCVal_t val;

    result = SMCReadKey(key, &val);
    if (result == kIOReturnSuccess) {
        // read succeeded - check returned value
        if (val.dataSize > 0) {
            return SMCNormalizeFloat(val);
        }
    }
    // read failed
    return -1.0;
}

double SMCNormalizeFloat(SMCVal_t val)
{
    if (strcmp(val.dataType, DATATYPE_SP78) == 0) {
        return ((SInt16)ntohs(*(UInt16*)val.bytes)) / 256.0;
    }
    if (strcmp(val.dataType, DATATYPE_SP5A) == 0) {
        return ((SInt16)ntohs(*(UInt16*)val.bytes)) / 1024.0;
    }
    if (strcmp(val.dataType, DATATYPE_FPE2) == 0) {
        return ntohs(*(UInt16*)val.bytes) / 4.0;
    }
    if (strcmp(val.dataType, DATATYPE_FP88) == 0) {
        return ntohs(*(UInt16*)val.bytes) / 256.0;
    }
    return -1.f;
}

int SMCNormalizeInt(SMCVal_t val)
{
    if (strcmp(val.dataType, DATATYPE_UINT8) == 0 || strcmp(val.dataType, DATATYPE_UINT16) == 0 || strcmp(val.dataType, DATATYPE_UINT32) == 0) {
        return (int)_strtoul((char*)val.bytes, val.dataSize, 10);
    }
    if (strcmp(val.dataType, DATATYPE_SI16) == 0) {
        return ntohs(*(SInt16*)val.bytes);
    }

    if (strcmp(val.dataType, DATATYPE_HEX) == 0 || strcmp(val.dataType, DATATYPE_FLAG) == 0) {
        printf("Hex value = 0x");
        int i;
        for (i = 0; i < val.dataSize; i++)
            printf("%02x ", (unsigned char)val.bytes[i]);
        printf("\n");
        return (int)_strtoul((char*)val.bytes, val.dataSize, 10);
    }
    return -1;
}

char* SMCNormalizeText(SMCVal_t val)
{
    char result[val.dataSize + 2];
    if (strcmp(val.dataType, DATATYPE_CH8) == 0) {
        int i;
        for (i = 0; i < val.dataSize; i++) {
            result[i] = (unsigned char)val.bytes[i];
        }
        result[i + 1] = 0;
        return strdup(result);
    }

    // convert anything else to text
    double f = SMCNormalizeFloat(val);
    if (f != -1.0) {
        snprintf(result, 10, "%0.1f", f);
        return strdup(result);
    }
    int i = SMCNormalizeInt(val);
    if (i != -1) {
        snprintf(result, 10, "%d", i);
        return strdup(result);
    }

    return strdup(result);
}

kern_return_t SMCPrintAll(void)
{
    kern_return_t result;
    SMCKeyData_t inputStructure;
    SMCKeyData_t outputStructure;

    int totalKeys, i;
    UInt32Char_t key;
    SMCVal_t val;

    totalKeys = SMCReadIndexCount();
    printf("Total keys = %d\n", totalKeys);

    for (i = 0; i < totalKeys; i++) {
        memset(&inputStructure, 0, sizeof(SMCKeyData_t));
        memset(&outputStructure, 0, sizeof(SMCKeyData_t));
        memset(&val, 0, sizeof(SMCVal_t));

        inputStructure.data8 = SMC_CMD_READ_INDEX;
        inputStructure.data32 = i;

        result = SMCCall(KERNEL_INDEX_SMC, &inputStructure, &outputStructure);
        if (result != kIOReturnSuccess)
            continue;

        _ultostr(key, outputStructure.key);

        result = SMCReadKey(key, &val);
        if (result != kIOReturnSuccess)
            continue;

        char* txt = SMCNormalizeText(val);
        printf("key = %s type = %s value = %s\n", key, val.dataType, txt);
    }

    return kIOReturnSuccess;
}

double SMCGetDouble(char* key)
{
    SMCVal_t val;
    kern_return_t result;

    result = SMCReadKey(key, &val);
    if (result == kIOReturnSuccess) {
        // read succeeded - check returned value
        if (val.dataSize > 0) {
            return SMCNormalizeFloat(val);
        }
    }
    // read failed
    return -1.0;
}

float SMCGetFanRPM(char* key)
{
    SMCVal_t val;
    kern_return_t result;

    result = SMCReadKey(key, &val);
    if (result == kIOReturnSuccess) {
        // read succeeded - check returned value
        if (val.dataSize > 0) {
            return SMCNormalizeFloat(val);
        }
    }
    // read failed
    return -1.f;
}

double SMCGetPower(char* key)
{
    SMCVal_t val;
    kern_return_t result;

    result = SMCReadKey(key, &val);
    if (result == kIOReturnSuccess) {
        // read succeeded - check returned value
        if (val.dataSize > 0) {
            return SMCNormalizeFloat(val);
        }
    }
    // read failed
    return -1.f;
}

=======
>>>>>>> ef86749 (resolve conflicts)
double convertToFahrenheit(double celsius)
{
    return (celsius * (9.0 / 5.0)) + 32.0;
}

double readTemperature(char* key, char scale)
{
    double temperature = SMCGetDouble(key);
    if (scale == 'F') {
        temperature = convertToFahrenheit(temperature);
    }
    return temperature;
}

void readAndPrintTemperature(char* title, bool show_title, char* key, char scale, bool show_scale)
{
    double temperature = SMCGetDouble(key);
    if (scale == 'F') {
        temperature = convertToFahrenheit(temperature);
    }

    double temperature = readTemperature(key, scale);

    if (show_scale) {
        printf("%s%0.1f °%c\n", title, temperature, scale);
    } else {
        printf("%s%0.1f\n", title, temperature);
    }
    readAndPrintTemperature(title, SMC_KEY_CPU_TEMP, scale);
}

void readAndPrintGpuTemp(bool show_title, char scale)
{
    char* title = "";
    if (show_title) {
        title = "GPU: ";
    }
    readAndPrintTemperature(title, SMC_KEY_GPU_TEMP, scale);
}

// Requires SMCOpen()
void readAndPrintTemp(char* key, char scale)
{
    double temperature = SMCGetDouble(key);
    if (scale == 'F') {
        temperature = convertToFahrenheit(temperature);
    }
    printf("%s = %0.1f °%c\n", key, temperature, scale);
}

// Requires SMCOpen()
void readAndPrintRawValue(char* key)
{
    SMCVal_t val;
    kern_return_t result;
    double f;
    int i;
    char* c;

    result = SMCReadKey(key, &val);
    if (result == kIOReturnSuccess) {
        // read succeeded - check returned value
        if (val.dataSize > 0) {
            f = SMCNormalizeFloat(val);
            if (f != -1.0) {
                printf("%s = %0.1f\n", key, f);
                return;
            }
            i = SMCNormalizeInt(val);
            if (i != -1) {
                printf("%s = %d\n", key, i);
                return;
            }
            c = SMCNormalizeText(val);
            if (strcmp(c, "") != 0) {
                printf("%s = %s\n", key, c);
                return;
            }
        }
    }
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
        totalFans = SMCNormalizeInt(val);

        printf("Num fans: %d\n", totalFans);
        for (i = 0; i < totalFans; i++) {
            sprintf(key, "F%dID", i);
            result = SMCReadKey(key, &val);
            if (result != kIOReturnSuccess) {
                continue;
            }
            char* name = val.bytes + 4;

            sprintf(key, "F%dAc", i);
            double actual_speed = SMCGetDouble(key);
            if (actual_speed < 0.f) {
                continue;
            }

            sprintf(key, "F%dMn", i);
            double minimum_speed = SMCGetDouble(key);
            if (minimum_speed < 0.f) {
                continue;
            }

            sprintf(key, "F%dMx", i);
            double maximum_speed = SMCGetDouble(key);
            if (maximum_speed < 0.f) {
                continue;
            }

            double rpm = actual_speed - minimum_speed;
            if (rpm < 0.f) {
                rpm = 0.f;
            }
            double pct = rpm / (maximum_speed - minimum_speed);

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

void readAndPrintAll(void)
{
    kern_return_t result;
    result = SMCPrintAll();
    if (result != kIOReturnSuccess) {
        printf("Error reading keys: %d", result);
    }
}

int main(int argc, char* argv[])
{
    char scale = 'C';
    bool show_scale = true;
    int cpu = 0;
    int fan = 0;
    int gpu = 0;
    int tmp = 0;
    int raw = 0;
    int all = 0;
    char* key;

    int c;
    while ((c = getopt(argc, argv, ":CFc:g:ft:r:aAh?")) != -1) {
        switch (c) {
        case 'F':
        case 'C':
            scale = c;
            break;
        case 'T':
            show_scale = false;
            break;
        case 'c':
            // optionally allow to pass the SMC Key (**TC0P**, TC0D, TCXC, ...)
            cpu = 1;
            key = optarg;
            break;
        case 'g':
            // optionally allow to pass the SMC Key (**TG0P**, TG0D, TCGC, ...)
            gpu = 1;
            key = optarg;
            break;
        case 'f':
            fan = true;
            break;
        case 't':
            tmp = 1;
            key = optarg;
            break;
        case 'r':
            raw = 1;
            key = optarg;
            break;
        // all option, see: https://github.com/hholtmann/smcFanControl/blob/875c68b0d36fbda40d2bf745fc43dcb40523360b/smc-command/smc.c#L485
        case 'A':
            all = 1;
            break;
        case ':':
            // optional arguments, set defaults
            switch (optopt) {
            case 'c':
                key = SMC_KEY_CPU_TEMP;
                break;
            case 'g':
                key = SMC_KEY_GPU_TEMP;
                break;
            default:
                printf("Error: '-%c' requires an argument", optopt);
                return -1;
            }
            break;
        case 'a':
            amb = 1;
            break;
        case 'h':
        case '?':
            printf("usage: osx-cpu-temp <options>\n");
            printf("Options:\n");
            printf("  -F       Display temperatures in degrees Fahrenheit.\n");
            printf("  -C       Display temperatures in degrees Celsius (Default).\n");
            printf("  -T       Do not display the units for temperatures.\n");
            printf("  -c [key] Display CPU temperature [of given key] (Default).\n");
            printf("  -g [key] Display GPU temperature [of given key].\n");
            printf("  -a       Display ambient temperature.\n");
            printf("  -f       Display fan speeds.\n");
            printf("  -t key   Display temperature of given SMC key.\n");
            printf("  -r key   Display raw value of given SMC key.\n");
            printf("  -A       Display ALL SMC keys\n")
            printf("  -h       Display this help.\n");
            printf("\nIf more than one of -c, -f, or -g are specified, titles will be added\n");
            return -1;
        }
    }

    if (!fan && !gpu && !tmp && !raw & !all) {
        cpu = 1;
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
    if (tmp) {
        readAndPrintTemp(key, scale);
    }
    if (raw) {
        readAndPrintRawValue(key);
    }
    if (all) {
        readAndPrintAll();
    }

    SMCClose();
    return 0;
}
