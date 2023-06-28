# OSX CPU Temp

Outputs current CPU temperature for OSX.

## Usage

### Compiling

```bash
make
```

### Running

```bash
./osx-cpu-temp
```

or

```bash
sudo make install # installs to /usr/local/bin
osx-cpu-temp
```

### Using clib

```bash
clib install lavoiesl/osx-cpu-temp
```

### Output example

```
61.8 °C
```

### Options

* Output
    * `-c` Display CPU temperature (Default).
    * `-g` Display GPU temperature.
    * `-a` Display ambient temperature.
    * `-f` Display fan speeds.
* Format
    * `-C` Display temperatures in degrees Celsius (Default).
    * `-F` Display temperatures in degrees Fahrenheit.
    * `-T` Do not display the units for temperatures.

### Explore keys
* see https://github.com/acidanthera/VirtualSMC/blob/master/Docs/SMCKeys.txt
* see https://app.assembla.com/wiki/show/fakesmc

Print the raw value of any key, if it exists
```shell script
osx-cpu-temp -r '#KEY' # number of keys
osx-cpu-temp -r TC0P # CPU proximity sensor
```

## Maintainer

Sébastien Lavoie <github@lavoie.sl>

### Source

Apple System Management Control (SMC) Tool
Copyright (C) 2006

### Inspiration

 * https://www.eidac.de/smcfancontrol/
 * https://github.com/hholtmann/smcFanControl/tree/master/smc-command
