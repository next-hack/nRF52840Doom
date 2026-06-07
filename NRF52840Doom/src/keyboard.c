/**
 *  Doom Port to the nRF52840 by Nicola Wrachien (next-hack in the comments)
 *
 *  This port is based on the excellent doomhack's GBA Doom Port,
 *  with Kippykip additions.
 *  
 *  Several data structures and functions have been optimized 
 *  to fit in only 256kB RAM of nRF52840 (GBA has 384 kB RAM). 
 *  Z-Depth Light has been restored with almost no RAM consumption!
 *  Tons of speed optimizations have been done, and the game now
 *  runs extremely fast, despite the much higher 3D resolution with
 *  respect to GBA.
 *
 *  Copyright (C) 2021 Nicola Wrachien (next-hack in the comments)
 *  on nRF52840 port.
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
 * Description: Keyboard handling functions for different types
 * (parallel, i2c and radio). For radio keyboard, this handles
 * also audio data transmission.
 */
#include "main.h"
#include "keyboard.h"
#include "printf.h"
#include "delay.h"
#if KEYBOARD == RADIO_KEYBOARD
    #include "pwm_audio.h"
    #define WIRELESS_DELAY_SAMPLES 50 // we send the packet every 50 samples, i.e. about every 5 ms. This must be EVEN number!
    #define MAX_RF_SAMPLES 52         // The packet is larger to account for any mismatch between frequencies and any possible delays 
    uint16_t rfAudioSamples[MAX_RF_SAMPLES];
    volatile uint8_t radioKeys[2];
    const uint8_t rf_address[] = RF_ADDRESS;
#endif
#if KEYBOARD == PARALLEL_KEYBOARD
    const uint8_t pins[] =
    {
        PIN_KEY_UP,
        PIN_KEY_DOWN,
        PIN_KEY_LEFT,
        PIN_KEY_RIGHT,
        PIN_KEY_USE,
        PIN_KEY_CHGW,
        PIN_KEY_ALT,
        PIN_KEY_FIRE
    };
    NRF_GPIO_Type* const ports[] =
    {
        GPIO_PORT(PORT_NUM_KEY_UP),
        GPIO_PORT(PORT_NUM_KEY_DOWN),
        GPIO_PORT(PORT_NUM_KEY_LEFT),
        GPIO_PORT(PORT_NUM_KEY_RIGHT),
        GPIO_PORT(PORT_NUM_KEY_USE),
        GPIO_PORT(PORT_NUM_KEY_CHGW),
        GPIO_PORT(PORT_NUM_KEY_ALT),
        GPIO_PORT(PORT_NUM_KEY_FIRE)
    };
    const key_t keys[] = 
    {
        KEY_UP,
        KEY_DOWN,
        KEY_LEFT,
        KEY_RIGHT,
        KEY_USE,
        KEY_CHGW,
        KEY_ALT,
        KEY_FIRE,
    };
#endif
#if KEYBOARD == I2C_KEYBOARD
volatile uint8_t i2cReadData;
uint8_t i2cWriteData[2];
void i2cSendAndWait(uint8_t addr, uint8_t value)
{
  // tx only
    NRF_TWIM1->EVENTS_STOPPED = 0;
    NRF_TWIM1->SHORTS = TWIM_SHORTS_LASTTX_STOP_Msk;
    i2cWriteData[0] = addr;
    i2cWriteData[1] = value;
    NRF_TWIM1->TXD.PTR = (uint32_t) i2cWriteData;
    NRF_TWIM1->TXD.MAXCNT = 2;
    NRF_TWIM1->RXD.MAXCNT = 0;
    NRF_TWIM1->TASKS_STARTTX = 1;
    // now wait till configuration has been sent
    while(!NRF_TWIM1->EVENTS_STOPPED);
    NRF_TWIM1->EVENTS_STOPPED = 0;
}
key_t updateI2cKeyboard(key_t *keys)
{
  // read old value and start a new read
    key_t k = i2cReadData;
    i2cWriteData[0] = MCP23008_GPIO;
    NRF_TWIM1->TXD.MAXCNT = 1;
    NRF_TWIM1->RXD.MAXCNT = 1;
    NRF_TWIM1->TXD.PTR = (uint32_t) i2cWriteData;
    NRF_TWIM1->RXD.PTR = (uint32_t) &i2cReadData;

    NRF_TWIM1->SHORTS = (TWIM_SHORTS_LASTTX_STARTRX_Enabled << TWIM_SHORTS_LASTTX_STARTRX_Pos)  
                      | (TWIM_SHORTS_LASTRX_STOP_Enabled << TWIM_SHORTS_LASTRX_STOP_Pos);
    NRF_TWIM1->TASKS_STARTTX = 1;
    *keys =  ~k;    // keys read 0 when pressed
}
void initI2cKeyboard()
{
    // configure GPIO as open drain
    GPIO_PORT(PORT_NUM_I2C_SCL)->PIN_CNF[PIN_NUM_I2C_SCL] =
        (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos) |
        (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
        (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
    GPIO_PORT(PORT_NUM_I2C_SDA)->PIN_CNF[PIN_NUM_I2C_SDA] =
        (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos) |
        (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
        (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
    // configure I2C PIN
    NRF_TWIM1->PSEL.SCL = (TWIM_PSEL_SCL_CONNECT_Connected << TWIM_PSEL_SCL_CONNECT_Pos) | (PIN_NUM_I2C_SCL << TWIM_PSEL_SCL_PIN_Pos) |
                          (PORT_NUM_I2C_SCL << TWIM_PSEL_SCL_PORT_Pos);
    NRF_TWIM1->PSEL.SDA = (TWIM_PSEL_SDA_CONNECT_Connected << TWIM_PSEL_SDA_CONNECT_Pos) | (PIN_NUM_I2C_SDA << TWIM_PSEL_SDA_PIN_Pos) |
                          (PORT_NUM_I2C_SDA << TWIM_PSEL_SDA_PORT_Pos);

    NRF_TWIM1->ADDRESS = EXPANDER_I2C_ADDRESS;

    //
    NRF_TWIM1->FREQUENCY = TWIM_FREQUENCY_FREQUENCY_K250 << TWIM_FREQUENCY_FREQUENCY_Pos;
    NRF_TWIM1->TXD.LIST = TWIM_TXD_LIST_LIST_ArrayList << TWIM_TXD_LIST_LIST_Pos;
    NRF_TWIM1->RXD.LIST = TWIM_RXD_LIST_LIST_ArrayList << TWIM_RXD_LIST_LIST_Pos;
    NRF_TWIM1->ENABLE = TWIM_ENABLE_ENABLE_Enabled << TWIM_ENABLE_ENABLE_Pos;
    // configure port expander
    // all input
    i2cSendAndWait(MCP23008_IODIR, 255);
    // all pull up
    i2cSendAndWait(MCP23008_GPPU, 255);
}
#endif
#if KEYBOARD == I2C_GAMEPAD
const uint8_t GamepadQTButtonMaskByte1 = (1 << GAMEPADQT_BUTTON_START);
const uint8_t GamepadQTButtonMaskByte3 = 
  (1 << GAMEPADQT_BUTTON_A) | (1 << GAMEPADQT_BUTTON_B) | (1 << GAMEPADQT_BUTTON_X) | (1 << GAMEPADQT_BUTTON_Y) | (1 << GAMEPADQT_BUTTON_SELECT);
const uint8_t GamepadQTButtonMaskFull[4] = {0, GamepadQTButtonMaskByte1, 0, GamepadQTButtonMaskByte3};
volatile uint8_t xReading[2];
volatile uint8_t yReading[2];
volatile uint8_t buttons[4];
uint8_t lastReadType;
uint32_t lastReadTime;
uint8_t i2cWriteData[6]; // max msg size is 2+4
void i2cSendAndWait(uint8_t regHigh, uint8_t regLow, const uint8_t *buf, uint8_t num)
{
    NRF_TWIM1->EVENTS_STOPPED = 0;
    NRF_TWIM1->SHORTS = TWIM_SHORTS_LASTTX_STOP_Msk;
    i2cWriteData[0] = regHigh;
    i2cWriteData[1] = regLow;
    for (int i = 0; i < num; i++)
    {
        i2cWriteData[2 + i] = buf[i];
    }
    NRF_TWIM1->TXD.PTR = (uint32_t) i2cWriteData;
    NRF_TWIM1->TXD.MAXCNT = 2 + num;
    NRF_TWIM1->RXD.MAXCNT = 0;
    NRF_TWIM1->TASKS_STARTTX = 1;
    // now wait till configuration has been sent
    while(!NRF_TWIM1->EVENTS_STOPPED);
    NRF_TWIM1->EVENTS_STOPPED = 0;
}
void i2cRead(uint8_t regHigh, uint8_t regLow, volatile uint8_t *buf, uint8_t num)
{
    i2cWriteData[0] = regHigh;
    i2cWriteData[1] = regLow;
    NRF_TWIM1->TXD.MAXCNT = 2;
    NRF_TWIM1->RXD.MAXCNT = num;
    NRF_TWIM1->TXD.PTR = (uint32_t) i2cWriteData;
    NRF_TWIM1->RXD.PTR = (uint32_t) buf;

    NRF_TWIM1->SHORTS = (TWIM_SHORTS_LASTTX_STARTRX_Enabled << TWIM_SHORTS_LASTTX_STARTRX_Pos)  
                      | (TWIM_SHORTS_LASTRX_STOP_Enabled << TWIM_SHORTS_LASTRX_STOP_Pos);
    NRF_TWIM1->TASKS_STARTTX = 1;
}
key_t updateI2cGamepad(key_t *keys)
{
    const uint16_t sensitivity = 50;

    // rate limit I2C reads, then continually just read from the buffers that will be updated async by TWIM
    if (NRF_TIMER3->CC[0] - lastReadTime > 250) { // us delay
      switch (lastReadType) {
        case 0: i2cRead(SEESAW_GPIO_BASE, SEESAW_GPIO_BULK, buttons, 4); break;
        case 1: i2cRead(SEESAW_ADC_BASE, SEESAW_ADC_CHANNEL_OFFSET + GAMEPADQT_JOYSTICK_X, xReading, 2); break;
        case 2: i2cRead(SEESAW_ADC_BASE, SEESAW_ADC_CHANNEL_OFFSET + GAMEPADQT_JOYSTICK_Y, yReading, 2); break;
      }

      lastReadTime = NRF_TIMER3->CC[0];
      lastReadType = (lastReadType + 1) % 3;
    }

    // read start button from byte 1 into upper byte of keys
    *keys = ((key_t)(~buttons[1] & GamepadQTButtonMaskByte1)) << 8;

    // read rest of buttons from byte 3 into lower byte of keys
    *keys |= (key_t)(~buttons[3] & GamepadQTButtonMaskByte3);

    // convert analog readings to digital input
    uint16_t x = (xReading[0] << 8) | xReading[1];
    if (x <= 1024) {
      *keys |= (x > (512 + sensitivity)) ? KEY_LEFT : 0;
      *keys |= (x < (512 - sensitivity)) ? KEY_RIGHT : 0;
    }

    uint16_t y = (yReading[0] << 8) | yReading[1];
    if (y <= 1024) {
      *keys |= (y < (512 - sensitivity)) ? KEY_UP : 0;
      *keys |= ( y > (512 + sensitivity)) ? KEY_DOWN : 0;
    }
}
void initI2cGamepad()
{
    // configure GPIO as open drain
    GPIO_PORT(PORT_NUM_I2C_SCL)->PIN_CNF[PIN_NUM_I2C_SCL] =
        (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos) |
        (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
        (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
    GPIO_PORT(PORT_NUM_I2C_SDA)->PIN_CNF[PIN_NUM_I2C_SDA] =
        (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos) |
        (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
        (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
    // configure I2C PIN
    NRF_TWIM1->PSEL.SCL = (TWIM_PSEL_SCL_CONNECT_Connected << TWIM_PSEL_SCL_CONNECT_Pos) | (PIN_NUM_I2C_SCL << TWIM_PSEL_SCL_PIN_Pos) |
                          (PORT_NUM_I2C_SCL << TWIM_PSEL_SCL_PORT_Pos);
    NRF_TWIM1->PSEL.SDA = (TWIM_PSEL_SDA_CONNECT_Connected << TWIM_PSEL_SDA_CONNECT_Pos) | (PIN_NUM_I2C_SDA << TWIM_PSEL_SDA_PIN_Pos) |
                          (PORT_NUM_I2C_SDA << TWIM_PSEL_SDA_PORT_Pos);

    NRF_TWIM1->ADDRESS = SEESAW_I2C_ADDRESS;

    //
    NRF_TWIM1->FREQUENCY = TWIM_FREQUENCY_FREQUENCY_K250 << TWIM_FREQUENCY_FREQUENCY_Pos;
    NRF_TWIM1->TXD.LIST = TWIM_TXD_LIST_LIST_ArrayList << TWIM_TXD_LIST_LIST_Pos;
    NRF_TWIM1->RXD.LIST = TWIM_RXD_LIST_LIST_ArrayList << TWIM_RXD_LIST_LIST_Pos;
    NRF_TWIM1->ENABLE = TWIM_ENABLE_ENABLE_Enabled << TWIM_ENABLE_ENABLE_Pos;

    // configure gamepad QT
    //i2cReadAndWait(SEESAW_STATUS_BASE, SEESAW_STATUS_HW_ID)
    //i2cReadAndWait(SEESAW_STATUS_BASE, SEESAW_STATUS_VERSION)
    i2cSendAndWait(SEESAW_GPIO_BASE, SEESAW_GPIO_DIRCLR_BULK, GamepadQTButtonMaskFull, 4);
    i2cSendAndWait(SEESAW_GPIO_BASE, SEESAW_GPIO_PULLENSET, GamepadQTButtonMaskFull, 4);
    i2cSendAndWait(SEESAW_GPIO_BASE, SEESAW_GPIO_BULK_SET, GamepadQTButtonMaskFull, 4);
    i2cSendAndWait(SEESAW_GPIO_BASE, SEESAW_GPIO_INTENSET, GamepadQTButtonMaskFull, 4);


    lastReadTime = NRF_TIMER3->CC[0];
    lastReadType = (lastReadTime + 1) % 3;

    // initiate readings for all input
    i2cRead(SEESAW_GPIO_BASE, SEESAW_GPIO_BULK, buttons, 4);
    delay(5);
    i2cRead(SEESAW_ADC_BASE, SEESAW_ADC_CHANNEL_OFFSET + GAMEPADQT_JOYSTICK_X, xReading, 2);
    delay(5);
    i2cRead(SEESAW_ADC_BASE, SEESAW_ADC_CHANNEL_OFFSET + GAMEPADQT_JOYSTICK_Y, yReading, 2);
    delay(500);
}
#endif
#if KEYBOARD == PARALLEL_KEYBOARD

void initParallelKeyboard()
{
    for (int i = 0; i < sizeof(pins); i++)
    {
        ports[i]->PIN_CNF[pins[i]] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                     (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) |
                                     (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
    }
}
#endif
#if KEYBOARD == RADIO_KEYBOARD
void initRadioKeyboard()
{
    // Configure radio with 2Mbit Nordic proprietary mode
    NRF_RADIO->MODE = RADIO_MODE_MODE_Nrf_1Mbit << RADIO_MODE_MODE_Pos;

    // Configure packet with no S0,S1 or Length fields and 8-bit preamble.
    NRF_RADIO->PCNF0 = (0 << RADIO_PCNF0_LFLEN_Pos) | (0 << RADIO_PCNF0_S0LEN_Pos) | (0 << RADIO_PCNF0_S1LEN_Pos) |
                       (RADIO_PCNF0_S1INCL_Automatic << RADIO_PCNF0_S1INCL_Pos) | (RADIO_PCNF0_PLEN_8bit << RADIO_PCNF0_PLEN_Pos);

    // Configure static payload length of sizeof(radioKeys) bytes. 4 bytes address, little endian with whitening enabled.
    NRF_RADIO->PCNF1 = (sizeof(radioKeys) << RADIO_PCNF1_MAXLEN_Pos) | (sizeof(radioKeys) << RADIO_PCNF1_STATLEN_Pos) | (4 << RADIO_PCNF1_BALEN_Pos) |
                       (RADIO_PCNF1_ENDIAN_Little << RADIO_PCNF1_ENDIAN_Pos) | (RADIO_PCNF1_WHITEEN_Enabled << RADIO_PCNF1_WHITEEN_Pos);

    // initialize whitening value
    NRF_RADIO->DATAWHITEIV = 0x55;

    // Configure address Prefix0 + Base0
    NRF_RADIO->BASE0 = *(uint32_t *)rf_address;
    NRF_RADIO->PREFIX0 = RF_ADDRESS_PREFIX << RADIO_PREFIX0_AP0_Pos;

    // Use logical address 0 (BASE0 + PREFIX0 byte 0)
    NRF_RADIO->RXADDRESSES = RADIO_RXADDRESSES_ADDR0_Enabled << RADIO_RXADDRESSES_ADDR0_Pos;

    // Initialize CRC (three bytes)
    NRF_RADIO->CRCCNF = (RADIO_CRCCNF_LEN_Three << RADIO_CRCCNF_LEN_Pos) | (RADIO_CRCCNF_SKIPADDR_Skip << RADIO_CRCCNF_SKIPADDR_Pos);
    NRF_RADIO->CRCPOLY = 0x00AAAAAA;
    NRF_RADIO->CRCINIT = 0x00345678;

    // Enable fast rampup, new in nRF52
    NRF_RADIO->MODECNF0 = (RADIO_MODECNF0_DTX_B0 << RADIO_MODECNF0_DTX_Pos) | (RADIO_MODECNF0_RU_Fast << RADIO_MODECNF0_RU_Pos);

    // receiving packets at 2400MHz
    NRF_RADIO->FREQUENCY = 0 << RADIO_FREQUENCY_FREQUENCY_Pos;
    // Configure address of the packet and logic address to use
    NRF_RADIO->PACKETPTR = (uint32_t)&radioKeys;

    // Configure shortcuts to start as soon as READY event is received, and disable radio as soon as packet is received.
    NRF_RADIO->SHORTS = (RADIO_SHORTS_READY_START_Enabled << RADIO_SHORTS_READY_START_Pos) | (RADIO_SHORTS_END_DISABLE_Enabled << RADIO_SHORTS_END_DISABLE_Pos);

    // the keyboard must receive also audio packet. Let's set.
    initWirelessAudio();
}
void updateRadioKeyboard(key_t * keys)
{
    // ok guys, the CRC is completely broken in NRF52xxx devices.
    //  I am not checking it, and I rely only on the address matching
    // if (NRF_RADIO->EVENTS_CRCOK)     // update only if CRC was ok.
    {
        // crc does not work properly so let's add a simple integrity check
        if ((key_t)radioKeys[0] == (key_t)(~radioKeys[1]))
        {
            *keys = radioKeys[0];
        }
    }
    // clear flag
    // NRF_RADIO->EVENTS_CRCOK = 0;
    // start RX. We are not waiting for event disabled, we are only receiving once each 29ms (or slower)
}

void initWirelessAudio()
{
    muteSound();
    NRF_TIMER1->CC[2] = 1;
    // set interrupt when compare 2 is reached
    NRF_TIMER1->INTENSET = TIMER_INTENSET_COMPARE2_Msk;
    NVIC_EnableIRQ(TIMER1_IRQn);
    NVIC_EnableIRQ(RADIO_IRQn);
}
void disableWirelessAudio()
{
    muteSound();
    delay(20); // wait 20 ms so that the mute is sent via wireless. TODO: find something better
    NVIC_DisableIRQ(TIMER1_IRQn);
}
void restoreWirelessAudio() { NVIC_EnableIRQ(TIMER1_IRQn); }
void TIMER1_IRQHandler(void)
{
    // get current index, so that we know from which sample we should send. Note, this IRQ has the highest priority, it might be delayed, but not interrupted.
    uint32_t currentCC0 = NRF_TIMER1->CC[0];
    // clear event
    NRF_TIMER1->EVENTS_COMPARE[2] = 0;
    // set when we are going to send the next samples
    NRF_TIMER1->CC[2] = (NRF_TIMER1->CC[2] + PWM_REFRESH_RATE * WIRELESS_DELAY_SAMPLES) & (AUDIO_BUFFER_LENGTH * PWM_REFRESH_RATE - 1);
    uint32_t currentIndex = currentCC0 / PWM_REFRESH_RATE;
    // fill the buffer
    for (int i = 0; i < MAX_RF_SAMPLES; i++)
    {
        rfAudioSamples[i] = audioBuffer[(currentIndex + i) & (AUDIO_BUFFER_LENGTH - 1)];
    }
    // Now, it could happen that the connection got broken or whatever. We cannot start TX
    // because we are in receive mode. Conversely, the gamepad will not start TX because it
    // is not receiving audio packet. Then force disable
    NRF_RADIO->TASKS_DISABLE = 1;
    //
    NRF_RADIO->PACKETPTR = (uint32_t)&rfAudioSamples[0];
    // set length to transmit
    NRF_RADIO->PCNF1 = (sizeof(rfAudioSamples) << RADIO_PCNF1_MAXLEN_Pos) | (sizeof(rfAudioSamples) << RADIO_PCNF1_STATLEN_Pos) | (4 << RADIO_PCNF1_BALEN_Pos) |
                       (RADIO_PCNF1_ENDIAN_Little << RADIO_PCNF1_ENDIAN_Pos) | (RADIO_PCNF1_WHITEEN_Enabled << RADIO_PCNF1_WHITEEN_Pos);
    // Clear disabled Event
    NRF_RADIO->EVENTS_DISABLED = 0;
    // enable transmit
    NRF_RADIO->TASKS_TXEN = 1;
    // Enable RADIO IRQ so that it will immediately listen to keyboard
    NRF_RADIO->INTENSET = RADIO_INTENSET_DISABLED_Msk;
}
void RADIO_IRQHandler(void)
{
    NRF_RADIO->INTENCLR = RADIO_INTENSET_DISABLED_Msk;
    // clear disabled event
    NRF_RADIO->EVENTS_DISABLED = 0;
    // set up packet pointer
    NRF_RADIO->PACKETPTR = (uint32_t)&radioKeys;
    // Configure static payload length of sizeof(radioKeys) bytes. 4 bytes address, little endian with whitening
    // enabled.
    NRF_RADIO->PCNF1 = (sizeof(radioKeys) << RADIO_PCNF1_MAXLEN_Pos) | (sizeof(radioKeys) << RADIO_PCNF1_STATLEN_Pos) | (4 << RADIO_PCNF1_BALEN_Pos) |
                       (RADIO_PCNF1_ENDIAN_Little << RADIO_PCNF1_ENDIAN_Pos) | (RADIO_PCNF1_WHITEEN_Enabled << RADIO_PCNF1_WHITEEN_Pos);
    NRF_RADIO->TASKS_RXEN = 1;
}
#endif
void initKeyboard()
{
#if KEYBOARD == PARALLEL_KEYBOARD
    initParallelKeyboard()
#elif KEYBOARD == I2C_KEYBOARD
    initI2cKeyboard();
#elif KEYBOARD == I2C_GAMEPAD
    initI2cGamepad();
#elif KEYBOARD == RADIO_KEYBOARD
    initRadioKeyboard();
#else
#error You should have a keyboard to play Doom, right?
#endif
}

void getKeys(key_t * keys)
{
#if KEYBOARD == PARALLEL_KEYBOARD
    key_t buttons = 0;
    for (int i = 0; i < 8; i++)
    {
        uint32_t pressed = !(ports[i]->IN & (1 << pins[i]));
        if (pressed)
        {
            buttons |= keys[i];
        }
    }
    *keys = buttons;
#elif KEYBOARD == I2C_KEYBOARD
    updateI2cKeyboard(keys);
#elif KEYBOARD == I2C_GAMEPAD
    updateI2cGamepad(keys);
#elif KEYBOARD == RADIO_KEYBOARD
    updateRadioKeyboard(keys);
#else
    #warning define some keyboard!
#endif
}