# nRF52 OGN Tracker

This is a port of my [OGN Tracker](https://github.com/pjalocha/ogn-tracker) project for nRF52 devices.
These devices receive GNSS position, exchange OGN/ADS-L/FANET/MESHT packets over an SX1262 radio,
show local status on an EPD/OLED display, internally log flights, and can expose NMEA data over BLE for apps such as XCSoar and SkyDemon.

![ThinkNode-M5, T-Beam Supreme, T-Echo and Wio Tracker running OGN-Tracker software](docs/images/trackers-photo.jpg)

## Support the Development

This project is open-source and developed voluntarily in spare time, without a
development budget. If you find it useful, please consider supporting the
development and research related to the Open Glider Network:

- OGN/ADS-L/FANET trackers
- OGN ground receivers which form the base of the network
- testing software and hardware experiments

**Donate via PayPal:**

https://paypal.me/paweljalocha

## Supported Hardware

- LilyGo T-Echo
- Seeed Studio Wio Tracker L1

The project uses PlatformIO for compilation, which is relatively easy to install and loads all compilers and tools automatically.
You don't actually need to compile yourself; you can download the correct binary file and copy it to your device.

## Main Features

- nRF52840 / Arduino / FreeRTOS firmware
- SX1262 radio support through RadioLib
- GNSS NMEA/UBX input
- transmits OGN, ADS-L (MDR+LDR+HDR), FANET and MESHT
- receives OGN and ADS-L
- relays selected messages
- Lookout algorithm for alerts
- E-paper display on T-Echo
- OLED display on Wio Tracker L1
- BLE serial-style NMEA service using `FFE0` / `FFE1`
- External flash logging and parameter storage
- UF2 generation after PlatformIO builds

## Build

Install PlatformIO, then build one of the environments:

```bash
pio run -e T-Echo
pio run -e Wio-Tracker
```

Generated firmware files are written below `.pio/build/<env>/`. The UF2 file is usually the most convenient artifact:

```text
.pio/build/T-Echo/firmware.uf2
.pio/build/Wio-Tracker/firmware.uf2
```

Ready-to-flash images for users are published under [GitHub Releases](https://github.com/pjalocha/nrf52-ogn-tracker/releases). See [`firmware/README.md`](firmware/README.md) for the release naming and versioning scheme.

## Firmware Downloads

Download the latest ready-to-use firmware from the [latest GitHub Release](https://github.com/pjalocha/nrf52-ogn-tracker/releases/latest).

Choose the UF2 file matching the hardware:

```text
nrf52-ogn-tracker-T-Echo-vX.Y.Z.uf2
nrf52-ogn-tracker-Wio-Tracker-vX.Y.Z.uf2
```

Copy the downloaded UF2 file to the device while it is in bootloader mode. The release also contains `SHA256SUMS` to verify the download.

## Flashing

You need to double-click the reset button on your device so it enters bootloader mode.
Attach your device to your PC and it should appear as a USB storage device;
then copy over the firmware image file, the one with .uf2 extension.

Use the file specific to your device, not just any .uf2 file.

Before installing a new Wio-Tracker firmware, look on the bootloader drive for
`CURRENT.UF2` and copy it to a safe place, preferably renaming it with the
device and date. This is the firmware currently installed on the device. If a
rollback is needed, enter bootloader mode again and copy that saved UF2 file
back to the drive. The file is available only with bootloaders that support
this feature, so verify that it is present before relying on the backup.

`CURRENT.UF2` backs up the application image, not flight logs stored in the
external flash. Do not format the external flash as part of a normal firmware
update.

## Serial Console

The monitor speed is `115200`:

```bash
pio device monitor -b 115200
```

Useful runtime controls:

- `Ctrl-C`: print current status and parameters
- `Ctrl-X` twice within one second: reboot
- `Ctrl-O` twice within one second: format external flash FAT filesystem

On Wio Tracker L1, tracker parameters are stored on the external flash as `/tracker.prm`.

## Wio-Tracker OLED menu

The Wio-Tracker L1 menu is operated with the joystick center button:

- Hold the joystick button for about 2 seconds to open the menu.
- Use joystick up/down to select an item; the main list wraps around.
- Press the joystick button briefly to enter an item or return to the menu list without saving.
- Hold the joystick button again to save or confirm. Confirmation screens use a short press to cancel.
- The page button briefly switches OLED pages. The joystick left/right directions also switch pages outside the menu.
- Hold the page button to lock or unlock the keypad. This prevents accidental joystick actions in a pocket or bag.

The menu includes aircraft type, address type and address, transmitter power, warning time, alert level, ghost mode, registration, pilot name, external-flash formatting, reset to defaults, and—when LoRaWAN is enabled—TTN registration with a QR code. Format the external flash after the first installation if flight logging is enabled.

## BLE NMEA Output

When `WITH_BLE_SPP` is enabled, the device advertises a BLE NMEA port compatible with apps that look for:

```text
Service UUID:        FFE0
Characteristic UUID: FFE1
```

The characteristic supports read, write, write-without-response, and notify.

## Board Notes

T-Echo:

- Long button press powers down through nRF52 System OFF.
- The EPD shows an OFF screen before shutdown.
- The button is configured as the wake source.

Wio Tracker L1:

- short press on the button next to the joystick switches the OLED pages
- long press on that page button locks or unlocks the keypad
- long press on the joystick center button opens the OLED menu

## Status

This is active firmware work. Hardware revisions, bootloaders, and flash parts may differ between devices, so verify pin maps and startup logs when bringing up a new board.
