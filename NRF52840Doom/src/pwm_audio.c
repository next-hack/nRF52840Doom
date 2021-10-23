/**
 *  Poor man's PWM audio driver for Doom Port to nRF52840.
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
 *  Simple audio driver. It supports multiple channels, but updateSound()
 *  shall be called frequently, otherwise sound glitches will occur.
 *  With a 1024 samples audio buffer, the minimum frame rate (and update rate)
 *  is (11025 / (1024 - AUDIO_BUFFER_DELAY)), i.e. 12 fps if AUDIO_BUFFER_DELAY is 100.
 *
 */
#pragma GCC optimize("Ofast") // we need to compile this code to be as fast as possible.
#include "pwm_audio.h"
#include "i_spi_support.h"
#include "nrf.h"
#include "sounds.h"
#include "w_wad.h"
#include "delay.h"
#include <string.h>
//
int16_t audioBuffer[AUDIO_BUFFER_LENGTH];

soundChannel_t soundChannels[MAX_CHANNELS];
void initPwmAudio()
{
    //
    if (PORT_NUM_AUDIO_OUT == 0)
    {
        NRF_P0->PIN_CNF[PIN_AUDIO_OUT] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                         (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                         (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
    }
    else
    {
        NRF_P1->PIN_CNF[PIN_AUDIO_OUT] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
                                         (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
                                         (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
    }
    // configure TIMER1 as a counter to keep track of how many samples did we sent
    NRF_TIMER1->BITMODE = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;
    NRF_TIMER1->SHORTS = TIMER_SHORTS_COMPARE1_CLEAR_Enabled << TIMER_SHORTS_COMPARE1_CLEAR_Pos;
    NRF_TIMER1->PRESCALER = 0 << TIMER_PRESCALER_PRESCALER_Pos; // 16 MHz
    NRF_TIMER1->MODE = TIMER_MODE_MODE_Counter << TIMER_MODE_MODE_Pos;
    NRF_TIMER1->CC[1] = AUDIO_BUFFER_LENGTH * PWM_REFRESH_RATE;
    NRF_TIMER1->TASKS_START = 1;
    // USE TIMER 2 to capture on TIMER1 CC[0] the current count - so that we do not need to trigger
    // task capture.
    NRF_PPI->FORK[0].TEP = (uint32_t)&NRF_TIMER1->TASKS_CAPTURE[0];
    // USE PPI to count each time a new PWM cycle starts
    NRF_PPI->CH[PPI_CH_AUDIO].EEP = (uint32_t)&NRF_PWM0->EVENTS_PWMPERIODEND;
    NRF_PPI->CH[PPI_CH_AUDIO].TEP = (uint32_t)&NRF_TIMER1->TASKS_COUNT;
    NRF_PPI->CHENSET = (1 << PPI_CH_AUDIO);
    //
    NRF_PWM0->PSEL.OUT[0] =
        (PORT_NUM_AUDIO_OUT << PWM_PSEL_OUT_PORT_Pos) | (PIN_AUDIO_OUT << PWM_PSEL_OUT_PIN_Pos) | (PWM_PSEL_OUT_CONNECT_Connected << PWM_PSEL_OUT_CONNECT_Pos);
    NRF_PWM0->ENABLE = (PWM_ENABLE_ENABLE_Enabled << PWM_ENABLE_ENABLE_Pos);
    NRF_PWM0->MODE = (PWM_MODE_UPDOWN_UpAndDown << PWM_MODE_UPDOWN_Pos);
    NRF_PWM0->PRESCALER = (PWM_PRESCALER_PRESCALER_DIV_1 << PWM_PRESCALER_PRESCALER_Pos);
    NRF_PWM0->COUNTERTOP = (PWM_COUNTER_TOP << PWM_COUNTERTOP_COUNTERTOP_Pos);
    NRF_PWM0->LOOP = (1 << PWM_LOOP_CNT_Pos);
    NRF_PWM0->DECODER = (PWM_DECODER_LOAD_Common << PWM_DECODER_LOAD_Pos) | (PWM_DECODER_MODE_RefreshCount << PWM_DECODER_MODE_Pos);
    for (int i = 0; i < 2; i++)
    {
        NRF_PWM0->SEQ[i].PTR = ((uint32_t)(audioBuffer) << PWM_SEQ_PTR_PTR_Pos);
        NRF_PWM0->SEQ[i].CNT = (AUDIO_BUFFER_LENGTH << PWM_SEQ_CNT_CNT_Pos);
        // one sample per each new PWM period
        NRF_PWM0->SEQ[i].REFRESH = PWM_REFRESH_RATE - 1;
        // no delay
        NRF_PWM0->SEQ[i].ENDDELAY = 0;
    }
    //
    NRF_PWM0->SHORTS = PWM_SHORTS_LOOPSDONE_SEQSTART0_Enabled << PWM_SHORTS_LOOPSDONE_SEQSTART0_Pos;
    // start
    NRF_PWM0->TASKS_SEQSTART[0] = 1;

    memset(soundChannels, 0, sizeof(soundChannels));
}
/**
 * Immediately shuts off sound
 */
void muteSound()
{
    for (int i = 0; i < AUDIO_BUFFER_LENGTH; i++)
    {
        audioBuffer[i] = ZERO_AUDIO_LEVEL;
    }
    for (int ch = 0; ch < MAX_CHANNELS; ch++)
    {
        soundChannels[ch].sfxIdx = 0;
        soundChannels[ch].volume = 0;
    }
}
/*
 * next-hack: poor's man mixer, NRF52840 version.
 *
 * There are N channels, each one can play one sample.
 * We create a buffer with 1024 samples, which is sent by DMA to the PWM.
 * Each sample of the buffer is 16-bit, because we need to mix all the channels.
 * The audio buffer is updated after all drawing operations have been done.
 *
 * Since doom's sample rate is 11025 Hz, then with a 1024 buffer, the minimum
 * required frame rate is about 11 fps, which is quite low and already unplayable on its own.
 *
 * The actual frame rate is unknown so we need to see where is the DMA source pointer, and
 * start updating some samples after its current position.
 *
 * This means a small delay for new samples, of some ms.
 *
 * We cannot see directly where we are in our current audio buffer. Therefore we will use a timer
 * (TIMER1) acting like a counter, which is triggered (using PPI) every time the PWM ends its PWM
 * cycle.
 *
 *
 */
void updateSound()
{
    // where are we in our circular buffer?
    uint32_t currentIdx = NRF_TIMER1->CC[0] / PWM_REFRESH_RATE;
    // we cannot start updating the audio buffer just on next sample (which will occur in about 90us
    // from now). if frame rate is high enough, then the audio buffer is valid several samples after
    // currentIdx. therefore we can start updating the audio buffer AUDIO_BUFFER_DELAY samples after
    // the current one. AUDIO_BUFFER_DELAY must be chosen so that we have enough time to read the
    // sample data from all the lumps and store to the buffer. This might take some time. The index
    // at which we recalculate/update audio samples is startIdx.
    uint32_t startIdx = (currentIdx + AUDIO_BUFFER_DELAY) & (AUDIO_BUFFER_LENGTH - 1);
    // The buffer is recalculated from startIdx up to the sample before currentIdx
    // As first thing, zeroize buffer;
    for (int i = startIdx; i != ((currentIdx - 1) & (AUDIO_BUFFER_LENGTH - 1)); i = (i + 1) & (AUDIO_BUFFER_LENGTH - 1))
    {
        audioBuffer[i] = ZERO_AUDIO_LEVEL;
    }
    // Now, for each channel we need to copy sample data.
    for (int ch = 0; ch < MAX_CHANNELS; ch++)
    {
        // channel active?
        if (soundChannels[ch].volume && soundChannels[ch].sfxIdx)
        {
            // get lump
            int lumpNum = p_wad_immutable_flash_data->soundLumps[soundChannels[ch].sfxIdx]; // W_CheckNumForName(S_sfx[soundChannels[ch].sfxIdx].name);
            if (lumpNum == -1)
                continue;
            const void * lumpPtr = W_CacheLumpNum(lumpNum);
            if (!isOnExternalFlash(lumpPtr))
                continue;
            // get sample size
            int32_t size = W_LumpLength(lumpNum);
            uint32_t samplesPlayed;
            // is this a new sample (not played before)? Then do not change the offset
            if (soundChannels[ch].lastAudioBufferIdx != 0xFFFF)
            {
                // otherwise, we need to calcualte how many sample do we have played since last
                // call. considering also that we are starting at startIdx.
                samplesPlayed = (startIdx - soundChannels[ch].lastAudioBufferIdx) & (AUDIO_BUFFER_LENGTH - 1);
                soundChannels[ch].offset = soundChannels[ch].offset + samplesPlayed;
            }
            // remember last index.
            soundChannels[ch].lastAudioBufferIdx = startIdx;
            // how many bytes do we need to read?
            int32_t sizeToRead = size - soundChannels[ch].offset;
            if (sizeToRead <= 0) // already outputted all samples? zeroize index and volume.
            {
                soundChannels[ch].sfxIdx = 0;
                soundChannels[ch].volume = 0;
                continue;
            }
            // if the number of samples to read exceed the size of the buffer we need to update
            // then crop it
            if (sizeToRead > AUDIO_BUFFER_LENGTH - AUDIO_BUFFER_DELAY)
            {
                sizeToRead = AUDIO_BUFFER_LENGTH - AUDIO_BUFFER_DELAY;
            }
            // create a temporary buffer. In stack is ok, we have a lot of it.
            uint8_t tmpBuffer[AUDIO_BUFFER_LENGTH - AUDIO_BUFFER_DELAY];
#if 0
            spiFlashSetAddress((uint32_t) lumpPtr + soundChannels[ch].offset);
            // read audio bytes
            spiFlashGetData(tmpBuffer, AUDIO_BUFFER_LENGTH - AUDIO_BUFFER_DELAY);
#else
            // reading through DMA is still faster than XIP
            NRF_QSPI->READ.CNT = (sizeToRead + 3) & ~3; // round up to nearest 4 boundary
            NRF_QSPI->READ.SRC = ((uint32_t)lumpPtr + soundChannels[ch].offset);
            NRF_QSPI->READ.DST = (uint32_t)tmpBuffer;
            NRF_QSPI->EVENTS_READY = 0;
            NRF_QSPI->TASKS_READSTART = 1;
            while (NRF_QSPI->EVENTS_READY == 0)
                ;
            NRF_QSPI->EVENTS_READY = 0;
#endif
            //
            uint32_t stopIdx = (startIdx + sizeToRead) & (AUDIO_BUFFER_LENGTH - 1);
            //
            uint8_t * p = tmpBuffer;
            for (int i = startIdx; i != stopIdx; i = (i + 1) & (AUDIO_BUFFER_LENGTH - 1))
            {
                // update audio buffer
                int32_t sampleValue = (0xFF & *p++) - 128;
                sampleValue *= soundChannels[ch].volume;
                // note: volume is between 0 and 127. Now we need to normalize
                sampleValue = sampleValue * ZERO_AUDIO_LEVEL / (128 * 128);
                audioBuffer[i] += sampleValue;
            }
        }
    }
}