#include "smc.c"

int main(int argc, char* argv[])
{
    char scale = 'C';
    int cpu = 0;
    int fan = 0;
    int gpu = 0;
    int amb = 0;

    int c;
    while ((c = getopt(argc, argv, "CFcfgah?")) != -1) {
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
        case 'a':
            amb = 1;
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
            printf("  -a  Display ambient temperature.\n");
            printf("  -g  Display GPU temperature.\n");
            printf("  -f  Display fan speeds.\n");
            printf("  -h  Display this help.\n");
            printf("\nIf more than one of -c, -f, or -g are specified, titles will be added\n");
            return -1;
            break;
        }
    }

    //default
    if(cpu+gpu+amb+fan < 1) cpu=1;
    int show_title = 1;

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
    if (amb) {
        readAndPrintAmbientTemp(show_title, scale);
    }

    SMCClose();
    return 0;
}