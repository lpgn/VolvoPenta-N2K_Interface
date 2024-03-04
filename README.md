# VolvoPenta-N2K_Interface
Interface to send the VP-CANbus-J1939 data to yout N2K network.
see: https://github.com/buhhe/VolvoPenta-N2K_Interface/blob/main/20211215%20J1939-N2K.pdf

## Build
### PlatformIO
Install [PlatformIO](https://platformio.org/).

You can install all dependencies and build the image with
```bash
pio run
```

Upload (and build, if necessary) the image to a ESP32 connected with USB
```bash
pio run --target upload
```

Monitor the output of the sketch with
```bash
pio device monitor
```

