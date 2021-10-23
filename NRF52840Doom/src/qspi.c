/**
 *  QSPI flash driver for Doom Port to nRF52840.
 *
 *  Copyright (C) 2021 by Nicola Wrachien (next-hack in the comments)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 *  DESCRIPTION:
 *  QSPI driver. Performs initialization, by enabling QSPI mode,
 *  also setting maximum drive strength, and flash size detection
 *
 */
#include "qspi.h"
#include "main.h"
#include "printf.h"
#include "i_memory.h"
#include "delay.h"

#define SPI_FLASH_WRITE_ENABLE_CMD 0x06
#define SPI_FLASH_PAGE_PROGRAM_CMD 0x02
#define SPI_FLASH_STATUS_REGISTER_READ_CMD 0x05
#define SPI_FLASH_CHIP_ERASE 0xC7
#define SPI_FLASH_SECTOR_ERASE 0x20
#define SPI_FLASH_MFG_ID 0x90
#define SPI_FLASH_STATUS_REGISTER_BUSY 1
#define SPI_FLASH_PAGE_SIZE 256
#define SPI_FLASH_WRITE_SR2_CMD 0x31
#define SPI_FLASH_READ_SR2_CMD 0x35
#define SPI_FLASH_WRITE_SR3_CMD 0x11
#define SPI_FLASH_VOLATILE_SR_WRITE_ENABLE_CMD 0x50
// ID for common flash sizes
#define ID_4M 0x15
#define ID_8M 0x16
#define ID_16M 0x17

//
#define FLASH_NCS_LOW() do{ NRF_P0->OUTCLR = (1 << PIN_FLASHSPI_NCS); } while(0)
#define FLASH_NCS_HIGH() do{ NRF_P0->OUTSET = (1 << PIN_FLASHSPI_NCS); } while(0)

//
static uint32_t flashSize = 0;
#if USE_SPI_FOR_QSPI_INIT
static void spiFlashWaitBusy();
static uint8_t spiFlashRead(volatile uint8_t byte)
{
    volatile uint8_t result;
    NRF_SPIM0->RXD.PTR = (uint32_t) &result;
    NRF_SPIM0->TXD.PTR = (uint32_t) &byte;
    NRF_SPIM0->RXD.MAXCNT = 1;
    NRF_SPIM0->TXD.MAXCNT = 1;
    NRF_SPIM0->TASKS_START = 1;
    while (0 == NRF_SPIM0->EVENTS_END)
    {
        // wait
    }
    NRF_SPIM0->EVENTS_END = 0;
    return result;
}
static void qspiSendCommandSpiMode(uint32_t txSize, uint32_t rxSize, const uint8_t * out, uint8_t * answer)
{
    FLASH_NCS_LOW();
    NRF_SPIM0->RXD.PTR = (uint32_t) answer;
    NRF_SPIM0->TXD.PTR = (uint32_t) out;
    NRF_SPIM0->RXD.MAXCNT = rxSize;
    NRF_SPIM0->TXD.MAXCNT = txSize;
    NRF_SPIM0->TASKS_START = 1;
    while (NRF_SPIM0->EVENTS_END == 0)
    {
        // wait
    }
    NRF_SPIM0->EVENTS_END = 0;
    FLASH_NCS_HIGH();
}

static void spiFlashDriveStrength()
{
    // set enable write of volatile SR
    FLASH_NCS_LOW();
    // enable volatile SR write
    spiFlashRead(SPI_FLASH_VOLATILE_SR_WRITE_ENABLE_CMD);
    FLASH_NCS_HIGH();
    FLASH_NCS_LOW();
    spiFlashRead(SPI_FLASH_WRITE_SR3_CMD);
    spiFlashRead(0x0);
    FLASH_NCS_HIGH();
    spiFlashWaitBusy();
}

static void spiFlashWaitBusy()
{
    volatile uint8_t result;
    FLASH_NCS_LOW();
    result = spiFlashRead(SPI_FLASH_STATUS_REGISTER_READ_CMD);
    do
    {
        result = spiFlashRead(0xFF);

    } while (result & SPI_FLASH_STATUS_REGISTER_BUSY);
    FLASH_NCS_HIGH();
}
static void spiFlashWriteEnable()
{
    FLASH_NCS_LOW();
    spiFlashRead(SPI_FLASH_WRITE_ENABLE_CMD);
    FLASH_NCS_HIGH();
}
static uint8_t spiFlashEnableQSPI()
{
    // set enable write of volatile SR
    FLASH_NCS_LOW();
    // enable volatile SR write
    spiFlashRead(SPI_FLASH_VOLATILE_SR_WRITE_ENABLE_CMD);
    FLASH_NCS_HIGH();
    FLASH_NCS_LOW();
    spiFlashRead(SPI_FLASH_WRITE_SR2_CMD);
    // enable QSPI
    spiFlashRead(0x02);
    FLASH_NCS_HIGH();
    spiFlashWaitBusy();
    // read back
    FLASH_NCS_LOW();
    spiFlashRead(SPI_FLASH_READ_SR2_CMD);
    // read data
    uint8_t sr2 = spiFlashRead(0x00);
    FLASH_NCS_HIGH();
    return sr2;
}
#endif
static uint8_t qspiCustomSendOpCodeAndOneByte(uint8_t opCode, uint8_t param)
{
    NRF_QSPI->CINSTRDAT0 = param;                                                      // param is 0
    NRF_QSPI->CINSTRCONF = opCode                                                      // opcode
                           | (QSPI_CINSTRCONF_LENGTH_2B << QSPI_CINSTRCONF_LENGTH_Pos) // send opcode + data 0
                           | (QSPI_CINSTRCONF_LIO2_Msk)                                // line 2 high
                           | (QSPI_CINSTRCONF_LIO3_Msk);                               // line 3 high
    while (!NRF_QSPI->EVENTS_READY)
    {
    }
    NRF_QSPI->EVENTS_READY = 0;
    return NRF_QSPI->CINSTRDAT0;
}

static void qspiWaitFlashReady()
{
    uint8_t sr;
    do
    {
        sr = qspiCustomSendOpCodeAndOneByte(SPI_FLASH_STATUS_REGISTER_READ_CMD, 0);
    } while (sr & 1);
}
inline uint32_t qspiFlashGetSize(void) { return flashSize; }
void qspiFlashProgram(uint32_t address, uint8_t * data, uint32_t size)
{
    NRF_QSPI->EVENTS_READY = 0;
    NRF_QSPI->WRITE.CNT = size;
    NRF_QSPI->WRITE.SRC = (uint32_t)data;
    NRF_QSPI->WRITE.DST = address;
    NRF_QSPI->TASKS_WRITESTART = 1;
    while (0 == NRF_QSPI->EVENTS_READY)
        ;
    NRF_QSPI->EVENTS_READY = 0;
    qspiWaitFlashReady();
}
void qspiFlashErasePage64k(uint32_t address)
{
    NRF_QSPI->EVENTS_READY = 0;
    NRF_QSPI->ERASE.LEN = QSPI_ERASE_LEN_LEN_64KB << QSPI_ERASE_LEN_LEN_Pos;
    NRF_QSPI->ERASE.PTR = address;
    NRF_QSPI->TASKS_ERASESTART = 1;
    while (!NRF_QSPI->EVENTS_READY)
        ;
    NRF_QSPI->EVENTS_READY = 0;
    qspiWaitFlashReady();
}
void qspiFlashErasePage4k(uint32_t address)
{
    NRF_QSPI->EVENTS_READY = 0;
    NRF_QSPI->ERASE.LEN = QSPI_ERASE_LEN_LEN_4KB << QSPI_ERASE_LEN_LEN_Pos;
    NRF_QSPI->ERASE.PTR = address;
    NRF_QSPI->TASKS_ERASESTART = 1;
    while (!NRF_QSPI->EVENTS_READY)
        ;
    NRF_QSPI->EVENTS_READY = 0;
    qspiWaitFlashReady();
}
void qspiFlashReadId(uint8_t * id, uint8_t * sz)
{
    // send custom instrucion
    NRF_QSPI->CINSTRDAT0 = 0;
    NRF_QSPI->CINSTRDAT1 = 0;
    //
    NRF_QSPI->CINSTRCONF = SPI_FLASH_MFG_ID                                            // opcode
                           | (QSPI_CINSTRCONF_LENGTH_6B << QSPI_CINSTRCONF_LENGTH_Pos) // send opcode + byte 0-4
                           | (QSPI_CINSTRCONF_LIO2_Msk)                                // line 2 high
                           | (QSPI_CINSTRCONF_LIO3_Msk);                               // line 3 high
    while (!NRF_QSPI->EVENTS_READY)
    {
    }
    NRF_QSPI->EVENTS_READY = 0;
    *id = NRF_QSPI->CINSTRDAT0 >> 24; // fourth data byte
    *sz = NRF_QSPI->CINSTRDAT1;       // fifth data byte
}
static void qspiFlashEnableVolatileSrWrite()
{
    // send custom instrucion
    //
    NRF_QSPI->CINSTRCONF = SPI_FLASH_VOLATILE_SR_WRITE_ENABLE_CMD                      // opcode
                           | (QSPI_CINSTRCONF_LENGTH_1B << QSPI_CINSTRCONF_LENGTH_Pos) // send opcode only
                           | (QSPI_CINSTRCONF_LIO2_Msk)                                // line 2 high
                           | (QSPI_CINSTRCONF_LIO3_Msk);                               // line 3 high
    while (!NRF_QSPI->EVENTS_READY)
    {
    }
    NRF_QSPI->EVENTS_READY = 0;
}
static void qspiFlashDriveStrength()
{
    // enable volatile SR write
    qspiFlashEnableVolatileSrWrite();
    //
    //  set full strength
    //
    qspiCustomSendOpCodeAndOneByte(SPI_FLASH_WRITE_SR3_CMD, 0);
    //
    qspiWaitFlashReady();
}
static uint8_t qspiFlashEnableQspiMode()
{
    // set enable write of volatile SR
    qspiFlashEnableVolatileSrWrite();
    // enable QSPI
    qspiCustomSendOpCodeAndOneByte(SPI_FLASH_WRITE_SR3_CMD, 0x02);
    //
    qspiWaitFlashReady();
    // read back
    uint8_t sr2 = qspiCustomSendOpCodeAndOneByte(SPI_FLASH_READ_SR2_CMD, 0x00);
    return sr2;
}

static void qspiEnable(uint32_t frequency)
{
    uint32_t prescaler;
    if (frequency <= 2000000)
    {
        prescaler = 15;
    }
    else
    {
        prescaler = 32000000 / frequency - 1;
    }
    NRF_QSPI->PSEL.CSN = (PIN_QSPI_NCS << QSPI_PSEL_CSN_PIN_Pos) | (QSPI_PSEL_CSN_CONNECT_Connected << QSPI_PSEL_CSN_CONNECT_Pos) |
                         (PORT_NUM_QSPI_NCS << QSPI_PSEL_CSN_PORT_Pos);

    NRF_QSPI->PSEL.SCK = (PIN_QSPI_CLK << QSPI_PSEL_SCK_PIN_Pos) | (QSPI_PSEL_SCK_CONNECT_Connected << QSPI_PSEL_SCK_CONNECT_Pos) |
                         (PORT_NUM_QSPI_CLK << QSPI_PSEL_SCK_PORT_Pos);

    NRF_QSPI->PSEL.IO0 =
        (PIN_QSPI_D0 << QSPI_PSEL_CSN_PIN_Pos) | (QSPI_PSEL_IO0_CONNECT_Connected << QSPI_PSEL_IO0_CONNECT_Pos) | (PORT_NUM_QSPI_D0 << QSPI_PSEL_IO0_PORT_Pos);

    NRF_QSPI->PSEL.IO1 =
        (PIN_QSPI_D1 << QSPI_PSEL_CSN_PIN_Pos) | (QSPI_PSEL_IO1_CONNECT_Connected << QSPI_PSEL_IO1_CONNECT_Pos) | (PORT_NUM_QSPI_D1 << QSPI_PSEL_IO1_PORT_Pos);

    NRF_QSPI->PSEL.IO2 =
        (PIN_QSPI_D2 << QSPI_PSEL_CSN_PIN_Pos) | (QSPI_PSEL_IO2_CONNECT_Connected << QSPI_PSEL_IO2_CONNECT_Pos) | (PORT_NUM_QSPI_D2 << QSPI_PSEL_IO2_PORT_Pos);

    NRF_QSPI->PSEL.IO3 =
        (PIN_QSPI_D3 << QSPI_PSEL_IO3_PIN_Pos) | (QSPI_PSEL_IO3_CONNECT_Connected << QSPI_PSEL_IO3_CONNECT_Pos) | (PORT_NUM_QSPI_D3 << QSPI_PSEL_IO3_PORT_Pos);

    NRF_QSPI->XIPOFFSET = EXT_FLASH_BASE;


    NRF_QSPI->IFCONFIG0 = (QSPI_IFCONFIG0_READOC_READ4IO << QSPI_IFCONFIG0_READOC_Pos) | (QSPI_IFCONFIG0_WRITEOC_PP4O << QSPI_IFCONFIG0_WRITEOC_Pos) |
                          (QSPI_IFCONFIG0_ADDRMODE_24BIT << QSPI_IFCONFIG0_ADDRMODE_Pos) | (QSPI_IFCONFIG0_DPMENABLE_Disable << QSPI_IFCONFIG0_DPMENABLE_Pos) |
                          (QSPI_IFCONFIG0_PPSIZE_256Bytes << QSPI_IFCONFIG0_PPSIZE_Pos);
    NRF_QSPI->IFCONFIG1 = (0 << QSPI_IFCONFIG1_SCKDELAY_Pos) // minimum CSN "high before low" time
                          | (QSPI_IFCONFIG1_DPMEN_Exit << QSPI_IFCONFIG1_DPMEN_Pos) | (QSPI_IFCONFIG1_SPIMODE_MODE0 << QSPI_IFCONFIG1_SPIMODE_Pos) |
                          (prescaler << QSPI_IFCONFIG1_SCKFREQ_Pos); // 32 MHz
    //
    NRF_QSPI->IFTIMING = 1 << QSPI_IFTIMING_RXDELAY_Pos; // 1 is ok. 0 or 2 are not.
    //
    NRF_QSPI->ENABLE = QSPI_ENABLE_ENABLE_Enabled << QSPI_ENABLE_ENABLE_Pos;
    //
    NRF_QSPI->EVENTS_READY = 0;
    //
    NRF_QSPI->TASKS_ACTIVATE = 1;
    while (0 == NRF_QSPI->EVENTS_READY)
        ;
    NRF_QSPI->EVENTS_READY = 0;
}
void qspiInit()
{
  // Some QSPI by default do not have QSPI mode enabled.
  // therefore we first need to make sure the QSPI mode is enabled.
  #if USE_SPI_FOR_QSPI_INIT
    // let us use SPI0, just to check, then we will disable it
    // enable short between end and start, so that continuous send is possible
    // SPI 0: clock connected,
    NRF_SPIM0->PSEL.SCK = (PORT_NUM_FLASHSPI_CLK << SPIM_PSEL_SCK_PORT_Pos) | (PIN_FLASHSPI_CLK << SPIM_PSEL_SCK_PIN_Pos) |
                          (SPIM_PSEL_SCK_CONNECT_Connected << SPIM_PSEL_SCK_CONNECT_Pos);
    // SPI 0 & 1: MISO connected. Note that in dual read mode, MOSI pin is an input
    NRF_SPIM0->PSEL.MISO = (PORT_NUM_FLASHSPI_MISO << SPIM_PSEL_MISO_PORT_Pos) | (PIN_FLASHSPI_MISO << SPIM_PSEL_MISO_PIN_Pos) |
                           (SPIM_PSEL_MISO_CONNECT_Connected << SPIM_PSEL_MISO_CONNECT_Pos);
    // SPI 0 & 1: MOSI DISCONNECTED
    NRF_SPIM0->PSEL.MOSI = (PORT_NUM_FLASHSPI_MOSI << SPIM_PSEL_MOSI_PORT_Pos) | (PIN_FLASHSPI_MOSI << SPIM_PSEL_MOSI_PIN_Pos) |
                           (SPIM_PSEL_MOSI_CONNECT_Connected << SPIM_PSEL_MOSI_CONNECT_Pos);
    // set frequency to 8 MHz. Sigh, so slow the maximum one!
    NRF_SPIM0->FREQUENCY = SPIM_FREQUENCY_FREQUENCY_M8 << SPIM_FREQUENCY_FREQUENCY_Pos;
    // ORC: don't care. Config: good the reset value.
    // Enable and configure easy dma for SPIM0
    NRF_SPIM0->RXD.LIST = SPIM_RXD_LIST_LIST_ArrayList << SPIM_RXD_LIST_LIST_Pos;
    NRF_SPIM0->TXD.LIST = SPIM_TXD_LIST_LIST_Disabled << SPIM_TXD_LIST_LIST_Pos;
    // Enable SPI
    NRF_SPIM0->ENABLE = SPIM_ENABLE_ENABLE_Enabled << SPIM_ENABLE_ENABLE_Pos;
    // set chip select and other pin drive strength
    GPIO_PORT(PORT_NUM_FLASHSPI_NCS)->PIN_CNF[PIN_FLASHSPI_NCS] =
        (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
        (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
        (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
    GPIO_PORT(PORT_NUM_FLASHSPI_CLK)->PIN_CNF[PIN_FLASHSPI_CLK] =
        (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
        (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
        (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
    GPIO_PORT(PORT_NUM_FLASHSPI_WP)->PIN_CNF[PIN_FLASHSPI_WP] =
        (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
        (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) |
        (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
    GPIO_PORT(PORT_NUM_FLASHSPI_HOLD)->PIN_CNF[PIN_FLASHSPI_HOLD] =
        (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
        (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) |
        (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
    GPIO_PORT(PORT_NUM_FLASHSPI_MOSI)->PIN_CNF[PIN_FLASHSPI_MOSI] =
        (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
        (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) |
        (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
    GPIO_PORT(PORT_NUM_FLASHSPI_MISO)->PIN_CNF[PIN_FLASHSPI_MISO] =
        (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
        (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) |
        (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);

    GPIO_PORT(PORT_NUM_FLASHSPI_HOLD)->OUTSET = (1 << PIN_FLASHSPI_HOLD);
    GPIO_PORT(PORT_NUM_FLASHSPI_WP)->OUTSET = (1 << PIN_FLASHSPI_WP);
    // send read ID command
    const uint8_t readMgdIdCmd[] = {SPI_FLASH_MFG_ID, 0, 0, 0, 0, 0};
    uint8_t answerBuffer[16];
    qspiSendCommandSpiMode(sizeof(readMgdIdCmd), sizeof(readMgdIdCmd), readMgdIdCmd, answerBuffer);
    uint8_t id = answerBuffer[4];
    uint8_t sz = answerBuffer[5];
    //
    FLASH_NCS_HIGH();
    flashSize = 4096 * 1024 << (sz - ID_4M);
    //
    printf("MFG ID: 0x%02x 0x%02x, size %d\r\n", id, sz, flashSize);
    //
    printf("Enabling QSPI\r\n");
    uint8_t sr2 = spiFlashEnableQSPI();
    printf("SR 2 is %x\r\n", sr2);
    // Set high drive strenght
    spiFlashDriveStrength();
    // now the flash can be used as QSPI flash. Let's disable SPIM0
    NRF_SPIM0->TASKS_STOP = 1;
    while (0 == NRF_SPIM0->EVENTS_STOPPED)
    {
    }
    NRF_SPIM0->EVENTS_STOPPED = 0;
    // Disable SPI
    NRF_SPIM0->ENABLE = 0;
    // now enable QSPI
    qspiEnable(32000000);
#else
    const uint8_t portNums[] = 
    {
        PORT_NUM_QSPI_D0, 
        PORT_NUM_QSPI_D1,
        PORT_NUM_QSPI_D2, 
        PORT_NUM_QSPI_D3,
        PORT_NUM_FLASHSPI_CLK, 
        PORT_NUM_FLASHSPI_NCS
    };
    const uint8_t gpioNums[] = 
    {
        PIN_QSPI_D0,
        PIN_QSPI_D1,
        PIN_QSPI_D2,
        PIN_QSPI_D3,
        PIN_QSPI_CLK,
        PIN_QSPI_NCS
    };

    // set fast gpio anyway
    for (int i = 0; i < sizeof(portNums); i++)
    {
        const uint32_t v = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
                           (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) |
                           (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
        if (portNums[i] == P1)
        {
            GPIO_PORT(P1)->PIN_CNF[gpioNums[i]] = v;
        }
        else
        {
            GPIO_PORT(P0)->PIN_CNF[gpioNums[i]] = v;
        }
    }
    // set low freq mode
    qspiEnable(2000000);
    //
    uint8_t id, sz;
    //
    qspiFlashReadId(&id, &sz);
    flashSize = 4096 * 1024 << (sz - ID_4M);
    //
    printf("MFG ID: 0x%02x 0x%02x, size %d\r\n", id, sz, flashSize);
    //
    printf("Enabling QSPI mode\r\n");
    uint8_t sr2 = qspiFlashEnableQspiMode();
    printf("SR 2 is %x\r\n", sr2);
    // Set high drive strength
    qspiFlashDriveStrength();
    // now the flash can be used as QSPI flash, at high speed mode.
    // let's deactivae it and start it again.
    NRF_QSPI->TASKS_DEACTIVATE = 1;
    delay(1); // wait 1 ms. Unluckily deactivate task does not trigger a ready event...
    NRF_QSPI->EVENTS_READY = 0;
    // set high freq mode
    qspiEnable(32000000);

#endif
    printf("Reading using XIP %x\r\n", *((uint32_t *)0x12000004));
}