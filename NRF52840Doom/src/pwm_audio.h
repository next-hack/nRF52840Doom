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
 *  see pw_audio.c
 *
 */
#ifndef SRC_PWM_AUDIO_H_
#define SRC_PWM_AUDIO_H_
#include "main.h"
#define MAX_CHANNELS 8
#define AUDIO_BUFFER_LENGTH 1024 // must be a multiple of 4
#define AUDIO_BUFFER_DELAY 100   // must be a multiple of 4
#define PIN_AUDIO_OUT 26
#define PORT_NUM_AUDIO_OUT 0
#define PWM_COUNTER_TOP (16000000 / (4 * 11025))
#define PPI_CH_AUDIO 1
#define PWM_REFRESH_RATE 2 // each sample lasts two PWM periods
#define ZERO_AUDIO_LEVEL (PWM_COUNTER_TOP / 2)
//
typedef struct
{
    uint16_t lastAudioBufferIdx;
    uint16_t offset;
    uint8_t sfxIdx;
    int8_t volume;
} soundChannel_t;
//
void initPwmAudio();
void updateSound();
void muteSound();
void muteSound();
extern int16_t audioBuffer[AUDIO_BUFFER_LENGTH];
extern soundChannel_t soundChannels[MAX_CHANNELS];
#endif /* SRC_PWM_AUDIO_H_ */