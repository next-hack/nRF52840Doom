#ifndef ST77XX_REGISTERS_H
#define ST77XX_REGISTERS_H
#define ST_CMD_DELAY 0x80 // special signifier for command lists

#define ST77XX_NOP 0x00
#define ST77XX_SWRESET 0x01
#define ST77XX_RDDID 0x04
#define ST77XX_RDDST 0x09

#define ST77XX_SLPIN 0x10
#define ST77XX_SLPOUT 0x11
#define ST77XX_PTLON 0x12
#define ST77XX_NORON 0x13

#define ST77XX_INVOFF 0x20
#define ST77XX_INVON 0x21
#define ST77XX_GAMSET 0x26
#define ST77XX_DISPOFF 0x28
#define ST77XX_DISPON 0x29
#define ST77XX_CASET 0x2A
#define ST77XX_RASET 0x2B
#define ST77XX_RAMWR 0x2C
#define ST77XX_RAMRD 0x2E

#define ST77XX_PTLAR 0x30
#define ST77XX_TEOFF 0x34
#define ST77XX_TEON 0x35
#define ST77XX_MADCTL 0x36
#define ST77XX_COLMOD 0x3A

#define ST77XX_MADCTL_MY 0x80
#define ST77XX_MADCTL_MX 0x40
#define ST77XX_MADCTL_MV 0x20
#define ST77XX_MADCTL_ML 0x10
#define ST77XX_MADCTL_RGB 0x00

#define ST77XX_RDID1 0xDA
#define ST77XX_RDID2 0xDB
#define ST77XX_RDID3 0xDC
#define ST77XX_RDID4 0xDD
// 7789 specific registers.
#define ST7789_PVGAMCTRL 0xE0
#define ST7789_NVGAMCTRL 0xE1
#define ST7789_GCTRL 0xB7    // Gate control
#define ST7789_VCOMS 0xBB    // VCOMS setting
#define ST7789_VDVVRHEN 0xC2 // VDV and VRH command enable
#define ST7789_VRHS 0xC3     // VRH set
#define ST7789_VDVSET 0xC4   // VDV setting
#define ST7789_VCMOFSET 0xC5 // VCOMS offset set
#define ST7789_FRCTR2 0xC6   // FR Control 2
#define ST7789_PWCTRL1 0xD0  // Power control 1

#endif