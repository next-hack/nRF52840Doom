/*********************************************************************
 *
 *  Keyboard and audio for nRF52840 Doom by Nicola Wrachien (next-hack)
 *
 *  Simple.
 *
 *  Copyright 2021, Nicola Wrachien
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
 *********************************************************************/
#include "main.h"
#include "nrf.h"
#include "printf.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
//
// Samples per audio packet. This must be at least as large as the value defined in the Doom device.
#define RF_AUDIO_SAMPLES 52
//
// Incoming audio data is written RF_AUDIO_DELAY samples after current pointer.
#define RF_AUDIO_DELAY 16
// number of audio samples in the local buffer
#define AUDIO_BUFFER_NUMBER_OF_SAMPLES 256  // this could be much smaller anyway, but all the RAM is unused...
//
#define PWM_OVERSAMPLING 4
#define PWM_COUNTER_TOP (16000000 / (PWM_OVERSAMPLING * 11025))

const uint8_t rfAddress[] = RF_ADDRESS;
volatile uint32_t currentAudioPos = 0;
uint16_t audioBuffer[AUDIO_BUFFER_NUMBER_OF_SAMPLES];
uint16_t rfAudioSamples[RF_AUDIO_SAMPLES];
uint8_t keys[2];
// Uart configuration.
void uartConfig()
{
    NRF_UART0->POWER = UART_POWER_POWER_Msk;
    // Configure the UARTE with no flow control, one parity bit and 115200 baud rate
    NRF_UART0->CONFIG = (UART_CONFIG_HWFC_Disabled << UART_CONFIG_HWFC_Pos) | (UART_CONFIG_PARITY_Excluded << UART_CONFIG_PARITY_Pos);

    NRF_UART0->BAUDRATE = UART_BAUDRATE_BAUDRATE_Baud115200 << UART_BAUDRATE_BAUDRATE_Pos;

    // Select TX and RX pins
    NRF_UART0->PSELTXD = PIN_TXD;
    NRF_UART0->PSELRXD = PIN_RXD;

    // Enable the UART (starts using the TX/RX pins)
    NRF_UART0->ENABLE = UART_ENABLE_ENABLE_Enabled << UART_ENABLE_ENABLE_Pos;
    // start tx
    NRF_UART0->TASKS_STARTTX = 1;
    NRF_UART0->TXD = '\r';
}
// for printf
void _putchar(char character)
{
    while (NRF_UART0->EVENTS_TXDRDY == 0)
    {
    }
    NRF_UART0->EVENTS_TXDRDY = 0;
    NRF_UART0->TXD = character;
}
// Simple main
void main(void)
{
    // Start HFCLK from crystal oscillator.
    NRF_CLOCK->TASKS_HFCLKSTART = 1;
    while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0)
        ;
    // init keys. From P0.00 to P0.07
    NRF_GPIO->DIRCLR = 0xFF;
    for (int i = 0; i < 8; i++)
    {
        NRF_GPIO->PIN_CNF[i] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
                               (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) |
                               (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
    }
    // PWM out
    NRF_GPIO->PIN_CNF[PWM_AUDIO_OUT_PIN] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                           (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) |
                                           (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);

    uartConfig();
    // print some stuff
    printf("nRF51822, RAM: %d\r\n", NRF_FICR->SIZERAMBLOCKS);
    // set timer so that it runs at 11025 Hz
    NRF_TIMER0->POWER = TIMER_POWER_POWER_Msk;
    NRF_TIMER0->BITMODE = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;
    NRF_TIMER0->SHORTS = TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos;
    NRF_TIMER0->PRESCALER = 0 << TIMER_PRESCALER_PRESCALER_Pos; // 16 MHz
    NRF_TIMER0->MODE = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;
    NRF_TIMER0->CC[0] = PWM_COUNTER_TOP; // TODO: +1 ? +0 ? -1 ?
    // enable interrupt
    NRF_TIMER0->INTENSET = TIMER_INTENSET_COMPARE0_Msk;
    // Config GPIOTE for PPI. All this to create a PWM...
    NRF_GPIOTE->POWER = 1;
    // toggle task
    NRF_GPIOTE->CONFIG[0] = (GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos) | (PWM_AUDIO_OUT_PIN << GPIOTE_CONFIG_PSEL_Pos) |
                            (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos) | (GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos);
    // set the pin toggle task
    NRF_PPI->CH[0].EEP = (uint32_t)&NRF_TIMER0->EVENTS_COMPARE[0];
    NRF_PPI->CH[0].TEP = (uint32_t)&NRF_GPIOTE->TASKS_OUT[0];
    // set the pin toggle task
    NRF_PPI->CH[1].EEP = (uint32_t)&NRF_TIMER0->EVENTS_COMPARE[1];
    NRF_PPI->CH[1].TEP = (uint32_t)&NRF_GPIOTE->TASKS_OUT[0];
    // Enable channel 0 and 1
    NRF_PPI->CHENSET = PPI_CHENSET_CH0_Msk | PPI_CHENSET_CH1_Msk;
    //
    NRF_RADIO->POWER = RADIO_POWER_POWER_Msk;
    // Configure radio with 2Mbit Nordic proprietary mode
    NRF_RADIO->MODE = RADIO_MODE_MODE_Nrf_1Mbit << RADIO_MODE_MODE_Pos;

    // Configure packet with no S0,S1 fields and 8-bit Length field. Preamble is always 8 bits on NRF51.
    NRF_RADIO->PCNF0 = (0 << RADIO_PCNF0_LFLEN_Pos) | (0 << RADIO_PCNF0_S0LEN_Pos) | (0 << RADIO_PCNF0_S1LEN_Pos);
    NRF_RADIO->PCNF1 = (sizeof(rfAudioSamples) << RADIO_PCNF1_MAXLEN_Pos) | (sizeof(rfAudioSamples) << RADIO_PCNF1_STATLEN_Pos) |
                       (4 << RADIO_PCNF1_BALEN_Pos) | // 5 bytes for address
                       (RADIO_PCNF1_ENDIAN_Little << RADIO_PCNF1_ENDIAN_Pos) | (RADIO_PCNF1_WHITEEN_Enabled << RADIO_PCNF1_WHITEEN_Pos);

    // initialize whitening value
    NRF_RADIO->DATAWHITEIV = 0x55;

    // Configure address Prefix0 + Base0
    NRF_RADIO->BASE0 = *(uint32_t *)rfAddress;
    NRF_RADIO->PREFIX0 = RF_ADDRESS_PREFIX << RADIO_PREFIX0_AP0_Pos;

    // Use logical address 0 (BASE0 + PREFIX0 byte 0)
    NRF_RADIO->TXADDRESS = 0 << RADIO_TXADDRESS_TXADDRESS_Pos;
    NRF_RADIO->RXADDRESSES = RADIO_RXADDRESSES_ADDR0_Msk;
    // Initialize CRC (two bytes)
    NRF_RADIO->CRCCNF = (RADIO_CRCCNF_LEN_Three << RADIO_CRCCNF_LEN_Pos) | (RADIO_CRCCNF_SKIPADDR_Skip << RADIO_CRCCNF_SKIPADDR_Pos);
    NRF_RADIO->CRCPOLY = 0x00AAAAAA;
    NRF_RADIO->CRCINIT = 0x00345678;


    // 0dBm output power, sending packets at 2400MHz
    NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_0dBm << RADIO_TXPOWER_TXPOWER_Pos;
    NRF_RADIO->FREQUENCY = 0 << RADIO_FREQUENCY_FREQUENCY_Pos;

    // Configure address of the packet
    NRF_RADIO->PACKETPTR = (uint32_t)&rfAudioSamples;

    // Configure shortcuts to start as soon as READY event is received, and disable radio as soon as packet is sent.
    NRF_RADIO->SHORTS = (RADIO_SHORTS_READY_START_Enabled << RADIO_SHORTS_READY_START_Pos) | (RADIO_SHORTS_END_DISABLE_Enabled << RADIO_SHORTS_END_DISABLE_Pos);
    NRF_RADIO->TASKS_START = 1;
    NRF_RADIO->INTENSET = RADIO_INTENSET_DISABLED_Msk;
    NVIC_SetPriority(RADIO_IRQn, 1);
    NVIC_EnableIRQ(RADIO_IRQn);
    NVIC_EnableIRQ(TIMER0_IRQn);
    NRF_RADIO->TASKS_RXEN = 1;
    // enable timer 1 so that when it overflows it will mute the audio. This is to protect ears...
    NRF_TIMER1->POWER = TIMER_POWER_POWER_Msk;
    NRF_TIMER1->BITMODE = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;
    NRF_TIMER1->SHORTS = TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos;
    NRF_TIMER1->PRESCALER = 3 << TIMER_PRESCALER_PRESCALER_Pos; // 16 MHz
    NRF_TIMER1->MODE = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;
    NRF_TIMER1->CC[0] = NO_RF_TIMEOUT_MICROS;
    NRF_TIMER1->TASKS_START = 1;
    // set silence.
    for (int i = 0; i < AUDIO_BUFFER_NUMBER_OF_SAMPLES; i++)
    {
        // set to midlevel
        audioBuffer[i] = PWM_COUNTER_TOP / 2;
    }
    // start timer used as PWNM
    NRF_TIMER0->TASKS_START = 1;
    // enable interrupt
    NRF_TIMER1->INTENSET = TIMER_INTENSET_COMPARE0_Msk;
    // Set timer 1 ISR to  lower priority.
    NVIC_SetPriority(TIMER1_IRQn, 2);
    NVIC_EnableIRQ(TIMER1_IRQn);
    // debug stuff.
    while (1)
    {
        // debug code. Won't have any effect on rf transmissio, as everything is interrupt driven
        uint8_t k;
        printf("Audio Samples:\r\n");
        uint16_t * pSample = rfAudioSamples;
        for (int i = 0; i < 8; i++)
        {
            printf("%04x ", *pSample++);
            printf("%04x ", *pSample++);
            printf("%04x ", *pSample++);
            printf("%04x ", *pSample++);
            printf("%04x ", *pSample++);
            printf("%04x ", *pSample++);
            printf("%04x ", *pSample++);
            printf("%04x\r\n", *pSample++);
        }
        k = ~NRF_GPIO->IN;
        printf("Keyboard! 0x%02x\r\n", k);
    }
}
void TIMER0_IRQHandler()
{
    // PWM frequency is set to PWM_OVERSAMPLING times the sample rate. This allows for an easier filtering.
    // The drawback is that this ISR is called PWM_OVERSAMPLING times for each sample: we can live with this.
    currentAudioPos++;
    currentAudioPos = currentAudioPos & (PWM_OVERSAMPLING * AUDIO_BUFFER_NUMBER_OF_SAMPLES - 1);
    // update duty cycle
    NRF_TIMER0->CC[1] = audioBuffer[currentAudioPos / PWM_OVERSAMPLING];
    NRF_TIMER0->EVENTS_COMPARE[0] = 0;
}
void TIMER1_IRQHandler()
{
    // we did not receive an audio packet for too long. Set to mute, to prevent nasty noises.
    for (int i = 0; i < AUDIO_BUFFER_NUMBER_OF_SAMPLES; i++)
    {
        // set to midlevel
        audioBuffer[i] = PWM_COUNTER_TOP / 2;
    }
    // clear event
    NRF_TIMER1->EVENTS_COMPARE[0] = 0;
}

void RADIO_IRQHandler()
{
    // got new packet. save it to the audio buffer and send the key status.
    NRF_RADIO->EVENTS_DISABLED = 0;
    // copy data to the correct position in the circular buffer
    uint32_t p = currentAudioPos / PWM_OVERSAMPLING;
    for (int i = 0; i < RF_AUDIO_SAMPLES; i++)
    {
        int16_t sample = rfAudioSamples[i] - PWM_COUNTER_TOP / 2;
        sample = sample >> 1;
        sample = sample + PWM_COUNTER_TOP / 2;
        audioBuffer[(AUDIO_BUFFER_NUMBER_OF_SAMPLES - 1) & (i + p + RF_AUDIO_DELAY)] = sample;
    }
    // Now transmit the key status
    keys[1] = NRF_GPIO->IN;
    keys[0] = ~keys[1];
    NRF_RADIO->PCNF1 = (sizeof(keys) << RADIO_PCNF1_MAXLEN_Pos) | (sizeof(keys) << RADIO_PCNF1_STATLEN_Pos) |
                       (4 << RADIO_PCNF1_BALEN_Pos) | // 5 bytes for address
                       (RADIO_PCNF1_ENDIAN_Little << RADIO_PCNF1_ENDIAN_Pos) | (RADIO_PCNF1_WHITEEN_Enabled << RADIO_PCNF1_WHITEEN_Pos);
    NRF_RADIO->PACKETPTR = (uint32_t)&keys;
    //
    NRF_RADIO->TASKS_TXEN = 1;

    while (!NRF_RADIO->EVENTS_DISABLED)
        ;
    NRF_RADIO->EVENTS_DISABLED = 0;
    NVIC_ClearPendingIRQ(RADIO_IRQn); // now me must clear the IRQ, as it occurred inside the ISR.
    NRF_RADIO->PACKETPTR = (uint32_t)&rfAudioSamples;
    // get back in receive mode.
    NRF_RADIO->PCNF1 = (sizeof(rfAudioSamples) << RADIO_PCNF1_MAXLEN_Pos) | (sizeof(rfAudioSamples) << RADIO_PCNF1_STATLEN_Pos) |
                       (4 << RADIO_PCNF1_BALEN_Pos) | // 5 bytes for address
                       (RADIO_PCNF1_ENDIAN_Little << RADIO_PCNF1_ENDIAN_Pos) | (RADIO_PCNF1_WHITEEN_Enabled << RADIO_PCNF1_WHITEEN_Pos);
    //
    // Clear timer 1
    NRF_TIMER1->TASKS_CLEAR = 1;
    //
    NRF_RADIO->TASKS_RXEN = 1;
}
/*************************** End of file ****************************/