# Asterix (Core 2 Duo)

## Programming

Asterix/Core 2 Duo mainboard has a B2B (Board-To-Board) connector that gives access to:

- MCU VDD, VUSB and GND
- MCU SWCLK, SWDIO and RESET
- Debug UART RX/TX

The connector part number is Molex 5050040812, and the pinout is as shown below.

```{figure} images/b2b-pinout.webp
Asterix B2B connector pinout
```

The "Core B2B v2" board has been designed as a companion programming board.
It is based on the [Raspberry Pi Debug Probe](https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html).
Software for the `Core B2B v2` board is available in the [Core B2B v2 repository](https://github.com/coredevices/asterix_b2b2_fw).
Below you can find a picture on how it is connected, and a list of its main features.

```{figure} images/asterix-programming.webp
Asterix and Core B2B v2
```

```{warning}
External connectors (SWD/UART) expose 1.8V signals.
Do not use any adapter that does not operate at 1.8V, or there is a **risk of damage**!
```

1. Asterix B2B connector
2. USB connector

   - Powers the board
   - Provides VUSB
   - Exposes a CMSIS-DAP device and a virtual COM port

3. Debug UART routing

   - L: External connector (5)
   - R: Embedded virtual COM port (2)

4. SWD routing

   - L: External SWD connector (6)
   - R: Embedded CMSIS-DAP (2)

5. Debug UART pins
6. External SWD connector
7. MCU Reset
8. VUSB switch

   - L: connected
   - R: disconnected

### Core B2B v2 Setup

Before plugging in the B2B board, make sure that your user is added to the `dialout` and `plugdev` groups.

To check your groups, enter the following command:
```bash
groups ${USER}
```

If the `dialout` or `plugdev` group is missing, add your user to a group using this command (replace `{your_group}` by the group name, either `dialout` or `plugdev`):
```bash
sudo gpasswd --add ${USER} {your_group}
```

Logout then log back in to your user session.

Create a file named `60-picotool.rules`and copy the following content:
```
# Copy this file to /etc/udev/rules.d/
# You can reload the udev rules with "udevadm control --reload"

SUBSYSTEM=="usb", \
    ATTRS{idVendor}=="2e8a", \
    ATTRS{idProduct}=="0003", \
    TAG+="uaccess", \
    MODE="660", \
    GROUP="plugdev"
SUBSYSTEM=="usb", \
    ATTRS{idVendor}=="2e8a", \
    ATTRS{idProduct}=="0009", \
    TAG+="uaccess", \
    MODE="660", \
    GROUP="plugdev"
SUBSYSTEM=="usb", \
    ATTRS{idVendor}=="2e8a", \
    ATTRS{idProduct}=="000a", \
    TAG+="uaccess", \
    MODE="660", \
    GROUP="plugdev"
SUBSYSTEM=="usb", \
    ATTRS{idVendor}=="2e8a", \
    ATTRS{idProduct}=="000c", \
    TAG+="uaccess", \
    MODE="660", \
    GROUP="plugdev"
SUBSYSTEM=="usb", \
    ATTRS{idVendor}=="2e8a", \
    ATTRS{idProduct}=="000f", \
    TAG+="uaccess", \
    MODE="660", \
    GROUP="plugdev"
```

Move this file into the folder located at `/etc/udev/rules.d/`. Replace the file if it already exists.

Enter the following command to reload the rules:
```bash
udevadm control --reload && udevadm trigger
```

Do not plug the board in just yet. Update the board firmware by following the procedure described [here](https://github.com/coredevices/asterix_b2b2_fw/releases)

### Usage and tips

When using the built-in programmer and USB-UART converter, make sure to place switches (3) and (4) to the right, as shown in the picture below.
It is also recommended to provide VUSB while programming so that the watch is always programmable regardless of the battery level.
For this to happen, place switch (8) to the left.

```{figure} images/b2bv2-switch-orientation.webp
Correct orientation of the switches when using built-in programmer, USB-UART converter and powering VUSB
```

- Please make sure to connect the B2Bv2 board in the **EXACT** way detailed in this document.
  If you connect it in any other orientation, your watch will be rendered unusable.
  You have been warned.
- You can check the path of the serial adapter by reading the output of `dmesg`.
  There should be a line stating the `tty*` allocated, most likely stating `ttyACM0`.
  In that case, you should use `/dev/ttyACM0`.
- If you are having trouble programming your watch, check the orientation of the switches.
  Sometimes they accidentally get switched to the wrong side during shipping.
- If your serial console is showing text but not accepting input, then your connector is likely not seated correctly.
  Press to click it in more.
