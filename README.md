## nRF52840 Doom
 Doom port to an nRF52840-based USB BLE dongle, the Adafruit CLUE board, and any device using Nordic Semiconductors nRF52840.
 Coded by Nicola Wrachien.

**Features**
- Based on the excellent doomhack's GBA Doom Port (https://github.com/doomhack/GBADoom) with some Kippykip's additions.

- Wireless gamepad with audio streaming like Wii remote.

- Extremely fast. Run almost always above 30 fps (except Episode 4 of Ultimate Doom).

- Additional gamepad support for Adafruit CLUE option (or other devices with enough I/Os)
  - Parallel Keyboard (each key routed to one GPIO).
  - MCP23008 I2C Port Expander.

- Tested with:
  - Shareware DOOM1.WAD.
  - Full Commercial DOOM.WAD
  - Ultimate Doom WAD.
  - Doom 2 WAD.

- Full Vanilla Render engine, **including Z-depth lighting**.

- Player's weapon also is subjected to ambient light.

- Monster behavior and sound propagation implemented.

- Sound FX support.

- Cheats implemented.

- NEW: 2022-04-24: Doom1.WAD v1.9 built-in demo support!

**What's missing?**
- Music.

- Demo playback: for now only Doom1.WAD v1.9 built-in demos are supported.

- Need to be debugged.

- Multiplayer not implemented.


## Controls:
**Strafe:** alt + Left-Right

**Automap:** use + change weapon

**Menu:** alt + use


## Hardware:
The schematics of the prototype, Adafruit micro:bit port expander board + audio and wireless gamepad are in the hardware subdirectory.

## Content of this repository:
- MCUDoomWadUtil: source of the WAD converter (Code::Blocks project). Note that GBADoom.wad must be put in the same directory as the executable.
- NRF52840Doom: the actual Doom port (Segger Embedded Studio Project)
- NRF52840DoomKeyboardOnNRF51822: wireless gamepad + audio (Segger Embedded Studio Project)
- hardware: schematics.

## Building, developing, etc:
See https://next-hack.com/index.php/2021/11/13/porting-doom-to-an-nrf52840-based-usb-bluetooth-le-dongle/