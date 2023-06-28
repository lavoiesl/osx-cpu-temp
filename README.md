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
    * `-c [key]` Print CPU temperature, optionally providing the SMC key.
    * `-g [key]` Print GPU temperature, optionally providing the SMC key.
    * `-a` Display ambient temperature.
    * `-f` Display fan speeds.
    * `-t key` Print temperature value of given SMC key.
    * `-r key` Print raw value of given SMC key.
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
