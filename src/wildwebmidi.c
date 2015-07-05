/*
 * wildmidi.c -- Midi Player using the WildMidi Midi Processing Library
 *
 * Copyright (C) WildMidi Developers 2001-2015
 *
 * This file is part of WildMIDI.
 *
 * WildMIDI is free software: you can redistribute and/or modify the player
 * under the terms of the GNU General Public License and you can redistribute
 * and/or modify the library under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either version 3 of
 * the licenses, or(at your option) any later version.
 *
 * WildMIDI is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License and
 * the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License and the
 * GNU Lesser General Public License along with WildMIDI.  If not,  see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <sys/types.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if !defined(_WIN32) && !defined(__DJGPP__) /* unix build */
static int msleep(unsigned long millisec);
#include <sys/ioctl.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#ifdef AUDIODRV_ALSA
#  include <alsa/asoundlib.h>
#elif defined AUDIODRV_OSS
#   if defined HAVE_SYS_SOUNDCARD_H
#   include <sys/soundcard.h>
#   elif defined HAVE_MACHINE_SOUNDCARD_H
#   include <machine/soundcard.h>
#   elif defined HAVE_SOUNDCARD_H
#   include <soundcard.h> /* less common, but exists. */
#   endif
#elif defined AUDIODRV_OPENAL
#   include <al.h>
#   include <alc.h>
#endif
#endif /* !_WIN32, !__DJGPP__ (unix build) */

#include "wildmidi_lib.h"
#include "filenames.h"

static void completeConversion(int status);

#include <emscripten.h>

struct _midi_test {
    uint8_t *data;
    uint32_t size;
};

/* scale test from 0 to 127
 * test a
 * offset 18-21 (0x12-0x15) - track size
 * offset 25 (0x1A) = bank number
 * offset 28 (0x1D) = patch number
 */
static uint8_t midi_test_c_scale[] = {
    0x4d, 0x54, 0x68, 0x64, 0x00, 0x00, 0x00, 0x06, /* 0x00    */
    0x00, 0x00, 0x00, 0x01, 0x00, 0x06, 0x4d, 0x54, /* 0x08    */
    0x72, 0x6b, 0x00, 0x00, 0x02, 0x63, 0x00, 0xb0, /* 0x10    */
    0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x90, 0x00, /* 0x18  C */
    0x64, 0x08, 0x80, 0x00, 0x00, 0x08, 0x90, 0x02, /* 0x20  D */
    0x64, 0x08, 0x80, 0x02, 0x00, 0x08, 0x90, 0x04, /* 0x28  E */
    0x64, 0x08, 0x80, 0x04, 0x00, 0x08, 0x90, 0x05, /* 0x30  F */
    0x64, 0x08, 0x80, 0x05, 0x00, 0x08, 0x90, 0x07, /* 0x38  G */
    0x64, 0x08, 0x80, 0x07, 0x00, 0x08, 0x90, 0x09, /* 0x40  A */
    0x64, 0x08, 0x80, 0x09, 0x00, 0x08, 0x90, 0x0b, /* 0x48  B */
    0x64, 0x08, 0x80, 0x0b, 0x00, 0x08, 0x90, 0x0c, /* 0x50  C */
    0x64, 0x08, 0x80, 0x0c, 0x00, 0x08, 0x90, 0x0e, /* 0x58  D */
    0x64, 0x08, 0x80, 0x0e, 0x00, 0x08, 0x90, 0x10, /* 0x60  E */
    0x64, 0x08, 0x80, 0x10, 0x00, 0x08, 0x90, 0x11, /* 0x68  F */
    0x64, 0x08, 0x80, 0x11, 0x00, 0x08, 0x90, 0x13, /* 0x70  G */
    0x64, 0x08, 0x80, 0x13, 0x00, 0x08, 0x90, 0x15, /* 0x78  A */
    0x64, 0x08, 0x80, 0x15, 0x00, 0x08, 0x90, 0x17, /* 0x80  B */
    0x64, 0x08, 0x80, 0x17, 0x00, 0x08, 0x90, 0x18, /* 0x88  C */
    0x64, 0x08, 0x80, 0x18, 0x00, 0x08, 0x90, 0x1a, /* 0x90  D */
    0x64, 0x08, 0x80, 0x1a, 0x00, 0x08, 0x90, 0x1c, /* 0x98  E */
    0x64, 0x08, 0x80, 0x1c, 0x00, 0x08, 0x90, 0x1d, /* 0xA0  F */
    0x64, 0x08, 0x80, 0x1d, 0x00, 0x08, 0x90, 0x1f, /* 0xA8  G */
    0x64, 0x08, 0x80, 0x1f, 0x00, 0x08, 0x90, 0x21, /* 0xB0  A */
    0x64, 0x08, 0x80, 0x21, 0x00, 0x08, 0x90, 0x23, /* 0xB8  B */
    0x64, 0x08, 0x80, 0x23, 0x00, 0x08, 0x90, 0x24, /* 0xC0  C */
    0x64, 0x08, 0x80, 0x24, 0x00, 0x08, 0x90, 0x26, /* 0xC8  D */
    0x64, 0x08, 0x80, 0x26, 0x00, 0x08, 0x90, 0x28, /* 0xD0  E */
    0x64, 0x08, 0x80, 0x28, 0x00, 0x08, 0x90, 0x29, /* 0xD8  F */
    0x64, 0x08, 0x80, 0x29, 0x00, 0x08, 0x90, 0x2b, /* 0xE0  G */
    0x64, 0x08, 0x80, 0x2b, 0x00, 0x08, 0x90, 0x2d, /* 0xE8  A */
    0x64, 0x08, 0x80, 0x2d, 0x00, 0x08, 0x90, 0x2f, /* 0xF0  B */
    0x64, 0x08, 0x80, 0x2f, 0x00, 0x08, 0x90, 0x30, /* 0xF8  C */
    0x64, 0x08, 0x80, 0x30, 0x00, 0x08, 0x90, 0x32, /* 0x100 D */
    0x64, 0x08, 0x80, 0x32, 0x00, 0x08, 0x90, 0x34, /* 0x108 E */
    0x64, 0x08, 0x80, 0x34, 0x00, 0x08, 0x90, 0x35, /* 0x110 F */
    0x64, 0x08, 0x80, 0x35, 0x00, 0x08, 0x90, 0x37, /* 0x118 G */
    0x64, 0x08, 0x80, 0x37, 0x00, 0x08, 0x90, 0x39, /* 0x120 A */
    0x64, 0x08, 0x80, 0x39, 0x00, 0x08, 0x90, 0x3b, /* 0X128 B */
    0x64, 0x08, 0x80, 0x3b, 0x00, 0x08, 0x90, 0x3c, /* 0x130 C */
    0x64, 0x08, 0x80, 0x3c, 0x00, 0x08, 0x90, 0x3e, /* 0x138 D */
    0x64, 0x08, 0x80, 0x3e, 0x00, 0x08, 0x90, 0x40, /* 0X140 E */
    0x64, 0x08, 0x80, 0x40, 0x00, 0x08, 0x90, 0x41, /* 0x148 F */
    0x64, 0x08, 0x80, 0x41, 0x00, 0x08, 0x90, 0x43, /* 0x150 G */
    0x64, 0x08, 0x80, 0x43, 0x00, 0x08, 0x90, 0x45, /* 0x158 A */
    0x64, 0x08, 0x80, 0x45, 0x00, 0x08, 0x90, 0x47, /* 0x160 B */
    0x64, 0x08, 0x80, 0x47, 0x00, 0x08, 0x90, 0x48, /* 0x168 C */
    0x64, 0x08, 0x80, 0x48, 0x00, 0x08, 0x90, 0x4a, /* 0x170 D */
    0x64, 0x08, 0x80, 0x4a, 0x00, 0x08, 0x90, 0x4c, /* 0x178 E */
    0x64, 0x08, 0x80, 0x4c, 0x00, 0x08, 0x90, 0x4d, /* 0x180 F */
    0x64, 0x08, 0x80, 0x4d, 0x00, 0x08, 0x90, 0x4f, /* 0x188 G */
    0x64, 0x08, 0x80, 0x4f, 0x00, 0x08, 0x90, 0x51, /* 0x190 A */
    0x64, 0x08, 0x80, 0x51, 0x00, 0x08, 0x90, 0x53, /* 0x198 B */
    0x64, 0x08, 0x80, 0x53, 0x00, 0x08, 0x90, 0x54, /* 0x1A0 C */
    0x64, 0x08, 0x80, 0x54, 0x00, 0x08, 0x90, 0x56, /* 0x1A8 D */
    0x64, 0x08, 0x80, 0x56, 0x00, 0x08, 0x90, 0x58, /* 0x1B0 E */
    0x64, 0x08, 0x80, 0x58, 0x00, 0x08, 0x90, 0x59, /* 0x1B8 F */
    0x64, 0x08, 0x80, 0x59, 0x00, 0x08, 0x90, 0x5b, /* 0x1C0 G */
    0x64, 0x08, 0x80, 0x5b, 0x00, 0x08, 0x90, 0x5d, /* 0x1C8 A */
    0x64, 0x08, 0x80, 0x5d, 0x00, 0x08, 0x90, 0x5f, /* 0x1D0 B */
    0x64, 0x08, 0x80, 0x5f, 0x00, 0x08, 0x90, 0x60, /* 0x1D8 C */
    0x64, 0x08, 0x80, 0x60, 0x00, 0x08, 0x90, 0x62, /* 0x1E0 D */
    0x64, 0x08, 0x80, 0x62, 0x00, 0x08, 0x90, 0x64, /* 0x1E8 E */
    0x64, 0x08, 0x80, 0x64, 0x00, 0x08, 0x90, 0x65, /* 0x1F0 F */
    0x64, 0x08, 0x80, 0x65, 0x00, 0x08, 0x90, 0x67, /* 0x1F8 G */
    0x64, 0x08, 0x80, 0x67, 0x00, 0x08, 0x90, 0x69, /* 0x200 A */
    0x64, 0x08, 0x80, 0x69, 0x00, 0x08, 0x90, 0x6b, /* 0x208 B */
    0x64, 0x08, 0x80, 0x6b, 0x00, 0x08, 0x90, 0x6c, /* 0x210 C */
    0x64, 0x08, 0x80, 0x6c, 0x00, 0x08, 0x90, 0x6e, /* 0x218 D */
    0x64, 0x08, 0x80, 0x6e, 0x00, 0x08, 0x90, 0x70, /* 0x220 E */
    0x64, 0x08, 0x80, 0x70, 0x00, 0x08, 0x90, 0x71, /* 0x228 F */
    0x64, 0x08, 0x80, 0x71, 0x00, 0x08, 0x90, 0x73, /* 0x230 G */
    0x64, 0x08, 0x80, 0x73, 0x00, 0x08, 0x90, 0x75, /* 0x238 A */
    0x64, 0x08, 0x80, 0x75, 0x00, 0x08, 0x90, 0x77, /* 0x240 B */
    0x64, 0x08, 0x80, 0x77, 0x00, 0x08, 0x90, 0x78, /* 0x248 C */
    0x64, 0x08, 0x80, 0x78, 0x00, 0x08, 0x90, 0x7a, /* 0x250 D */
    0x64, 0x08, 0x80, 0x7a, 0x00, 0x08, 0x90, 0x7c, /* 0x258 E */
    0x64, 0x08, 0x80, 0x7c, 0x00, 0x08, 0x90, 0x7d, /* 0x260 F */
    0x64, 0x08, 0x80, 0x7d, 0x00, 0x08, 0x90, 0x7f, /* 0x268 G */
    0x64, 0x08, 0x80, 0x7f, 0x00, 0x08, 0xff, 0x2f, /* 0x270   */
    0x00                                            /* 0x278   */
};

static struct _midi_test midi_test[] = {
    { midi_test_c_scale, 663 },
    { NULL, 0 }
};

static int midi_test_max = 1;

/*
 ==============================
 Audio Output Functions

 We have two 'drivers': first is the wav file writer which is
 always available. the second, if it is really compiled in,
 is the system audio output driver. only _one of the two_ can
 be active, not both.
 ==============================
 */

static unsigned int rate = 32072;

static int (*send_output)(int8_t *output_data, int output_size);
static void (*close_output)(void);
static void (*pause_output)(void);
static void (*resume_output)(void);
static int audio_fd = -1;

static void pause_output_nop(void) {
}
static void resume_output_nop(void) {
}

/*
 MIDI Output Functions
 */
static char midi_file[1024];

static int write_midi_output(void *output_data, int output_size) {
    struct stat st;

    if (midi_file[0] == '\0')
        return (-1);

/*
 * Test if file already exists
 */
    if (stat(midi_file, &st) == 0) {
        fprintf(stderr, "\rError: %s already exists\r\n", midi_file);
        return (-1);
    }

    audio_fd = open(midi_file, (O_RDWR | O_CREAT | O_TRUNC),
                                                            (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH));
    if (audio_fd < 0) {
        fprintf(stderr, "Error: unable to open file for writing (%s)\r\n", strerror(errno));
        return (-1);
    }

    if (write(audio_fd, output_data, output_size) < 0) {
        fprintf(stderr, "\nERROR: failed writing midi (%s)\r\n", strerror(errno));
        close(audio_fd);
        audio_fd = -1;
        return (-1);
    }

    close(audio_fd);
    audio_fd = -1;
    return (0);
}

/*
 Wav Output Functions
 */

static uint32_t wav_size;

static int write_wav_output(int8_t *output_data, int output_size);
static void close_wav_output(void);

static int open_wav_output(char* wav_file) {
    uint8_t wav_hdr[] = {
        0x52, 0x49, 0x46, 0x46, /* "RIFF"  */
        0x00, 0x00, 0x00, 0x00, /* riffsize: pcm size + 36 (filled when closing.) */
        0x57, 0x41, 0x56, 0x45, /* "WAVE"  */
        0x66, 0x6D, 0x74, 0x20, /* "fmt "  */
        0x10, 0x00, 0x00, 0x00, /* length of this RIFF block: 16  */
        0x01, 0x00,             /* wave format == 1 (WAVE_FORMAT_PCM)  */
        0x02, 0x00,             /* channels == 2  */
        0x00, 0x00, 0x00, 0x00, /* sample rate (filled below)  */
        0x00, 0x00, 0x00, 0x00, /* bytes_per_sec: rate * channels * format bytes  */
        0x04, 0x00,             /* block alignment: channels * format bytes == 4  */
        0x10, 0x00,             /* format bits == 16  */
        0x64, 0x61, 0x74, 0x61, /* "data"  */
        0x00, 0x00, 0x00, 0x00  /* datasize: the pcm size (filled when closing.)  */
    };

    if (wav_file[0] == '\0')
        return (-1);

    audio_fd = open(wav_file, (O_RDWR | O_CREAT | O_TRUNC),
                                                           (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH));
    if (audio_fd < 0) {
        fprintf(stderr, "Error: unable to open file for writing (%s)\r\n", strerror(errno));
        return (-1);
    } else {
        uint32_t bytes_per_sec;

        wav_hdr[24] = (rate) & 0xFF;
        wav_hdr[25] = (rate >> 8) & 0xFF;

        bytes_per_sec = rate * 4;
        wav_hdr[28] = (bytes_per_sec) & 0xFF;
        wav_hdr[29] = (bytes_per_sec >> 8) & 0xFF;
        wav_hdr[30] = (bytes_per_sec >> 16) & 0xFF;
        wav_hdr[31] = (bytes_per_sec >> 24) & 0xFF;
    }

    if (write(audio_fd, &wav_hdr, 44) < 0) {
        fprintf(stderr, "ERROR: failed writing wav header (%s)\r\n", strerror(errno));
        close(audio_fd);
        audio_fd = -1;
        return (-1);
    }

    wav_size = 0;
    send_output = write_wav_output;
    close_output = close_wav_output;
    pause_output = pause_output_nop;
    resume_output = resume_output_nop;
    return (0);
}

static int write_wav_output(int8_t *output_data, int output_size) {
#ifdef WORDS_BIGENDIAN
/* libWildMidi outputs host-endian, *.wav must have little-endian. */
    uint16_t *swp = (uint16_t *) output_data;
    int i = (output_size / 2) - 1;
    for (; i >= 0; --i) {
        swp[i] = (swp[i] << 8) | (swp[i] >> 8);
    }
#endif
    if (write(audio_fd, output_data, output_size) < 0) {
        fprintf(stderr, "\nERROR: failed writing wav (%s)\r\n", strerror(errno));
        close(audio_fd);
        audio_fd = -1;
        return (-1);
    }

    wav_size += output_size;
    return (0);
}

static void close_wav_output(void) {
    uint8_t wav_count[4];
    if (audio_fd < 0)
        return;

    printf("Finishing and closing wav output\r");
    wav_count[0] = (wav_size) & 0xFF;
    wav_count[1] = (wav_size >> 8) & 0xFF;
    wav_count[2] = (wav_size >> 16) & 0xFF;
    wav_count[3] = (wav_size >> 24) & 0xFF;
    lseek(audio_fd, 40, SEEK_SET);
    if (write(audio_fd, &wav_count, 4) < 0) {
        fprintf(stderr, "\nERROR: failed writing wav (%s)\r\n", strerror(errno));
        goto end;
    }

    wav_size += 36;
    wav_count[0] = (wav_size) & 0xFF;
    wav_count[1] = (wav_size >> 8) & 0xFF;
    wav_count[2] = (wav_size >> 16) & 0xFF;
    wav_count[3] = (wav_size >> 24) & 0xFF;
    lseek(audio_fd, 4, SEEK_SET);
    if (write(audio_fd, &wav_count, 4) < 0) {
        fprintf(stderr, "\nERROR: failed writing wav (%s)\r\n", strerror(errno));
        goto end;
    }

end:    printf("\n");
    if (audio_fd >= 0)
        close(audio_fd);
    audio_fd = -1;
}

#if defined AUDIODRV_OPENAL

#define NUM_BUFFERS 4

static ALCdevice *device;
static ALCcontext *context;
static ALuint sourceId = 0;
static ALuint buffers[NUM_BUFFERS];
static ALuint frames = 0;

#define open_audio_output open_openal_output

static void pause_output_openal(void) {
    alSourcePause(sourceId);
}

static int write_openal_output(int8_t *output_data, int output_size) {
    ALint processed, state;
    ALuint bufid;

    if (frames < NUM_BUFFERS) { /* initial state: fill the buffers */
        alBufferData(buffers[frames], AL_FORMAT_STEREO16, output_data,
                     output_size, rate);

        /* Now queue and start playback! */
        if (++frames == NUM_BUFFERS) {
            alSourceQueueBuffers(sourceId, frames, buffers);
            alSourcePlay(sourceId);
        }
        return 0;
    }

    /* Get relevant source info */
    alGetSourcei(sourceId, AL_SOURCE_STATE, &state);
    if (state == AL_PAUSED) { /* resume it, then.. */
        alSourcePlay(sourceId);
        if (alGetError() != AL_NO_ERROR) {
            fprintf(stderr, "\nError restarting playback\r\n");
            return (-1);
        }
    }

    processed = 0;
    while (processed == 0) { /* Wait until we have a processed buffer */
        alGetSourcei(sourceId, AL_BUFFERS_PROCESSED, &processed);
    }

    /* Unqueue and handle each processed buffer */
    alSourceUnqueueBuffers(sourceId, 1, &bufid);

    /* Read the next chunk of data, refill the buffer, and queue it
     * back on the source */
    alBufferData(bufid, AL_FORMAT_STEREO16, output_data, output_size, rate);
    alSourceQueueBuffers(sourceId, 1, &bufid);
    if (alGetError() != AL_NO_ERROR) {
        fprintf(stderr, "\nError buffering data\r\n");
        return (-1);
    }

    /* Make sure the source hasn't underrun */
    alGetSourcei(sourceId, AL_SOURCE_STATE, &state);
    /*printf("STATE: %#08x - %d\n", state, queued);*/
    if (state != AL_PLAYING) {
        ALint queued;

        /* If no buffers are queued, playback is finished */
        alGetSourcei(sourceId, AL_BUFFERS_QUEUED, &queued);
        if (queued == 0) {
            fprintf(stderr, "\nNo buffers queued for playback\r\n");
            return (-1);
        }

        alSourcePlay(sourceId);
    }

    return (0);
}

static void close_openal_output(void) {
    if (!context) return;
    printf("Shutting down sound output\r\n");
    alSourceStop(sourceId);         /* stop playing */
    alSourcei(sourceId, AL_BUFFER, 0);  /* unload buffer from source */
    alDeleteBuffers(NUM_BUFFERS, buffers);
    alDeleteSources(1, &sourceId);
    alcDestroyContext(context);
    alcCloseDevice(device);
    context = NULL;
    device = NULL;
    frames = 0;
}

static int open_openal_output(void) {
    /* setup our audio devices and contexts */
    device = alcOpenDevice(NULL);
    if (!device) {
        fprintf(stderr, "OpenAL: Unable to open default device.\r\n");
        return (-1);
    }

    context = alcCreateContext(device, NULL);
    if (context == NULL || alcMakeContextCurrent(context) == ALC_FALSE) {
        if (context != NULL)
            alcDestroyContext(context);
        alcCloseDevice(device);
        context = NULL;
        device = NULL;
        fprintf(stderr, "OpenAL: Failed to create the default context.\r\n");
        return (-1);
    }

    /* setup our sources and buffers */
    alGenSources(1, &sourceId);
    alGenBuffers(NUM_BUFFERS, buffers);

    send_output = write_openal_output;
    close_output = close_openal_output;
    pause_output = pause_output_openal;
    resume_output = resume_output_nop;
    return (0);
}
#else /* no audio output driver compiled in: */

#define open_audio_output open_noaudio_output
static int open_noaudio_output(void) {
    return -1;
}

#endif

/* no audio output driver compiled in: */

static void do_version(void) {
    printf("\nWildMidi %s Open Source Midi Sequencer\n", PACKAGE_VERSION);
    printf("Copyright (C) WildMIDI Developers 2001-2015\n\n");
    printf("WildMidi comes with ABSOLUTELY NO WARRANTY\n");
    printf("This is free software, and you are welcome to redistribute it under\n");
    printf("the terms and conditions of the GNU General Public License version 3.\n");
    printf("For more information see COPYING\n\n");
    printf("Report bugs to %s\n", PACKAGE_BUGREPORT);
    printf("WildMIDI homepage is at %s\n\n", PACKAGE_URL);
}

static char *config_file;

int wildwebmidi(char* midi_file, char* wav_file, int sleep) {

    #ifdef NODEJS
    // mount the current folder as a NODEFS instance
    // inside of emscripten
    EM_ASM(
        FS.mkdir('/working');
        FS.mount(NODEFS, { root: '.' }, '/working');
    );
    #endif

    struct _WM_Info *wm_info;
    int i, res;
    int option_index = 0;
    uint16_t mixer_options = 0;
    void *midi_ptr;
    uint8_t master_volume = 100;
    int8_t *output_buffer;
    uint32_t perc_play;
    uint32_t pro_mins;
    uint32_t pro_secs;
    uint32_t apr_mins;
    uint32_t apr_secs;
    char modes[5];
    uint32_t count_diff;
    uint8_t ch;
    uint8_t test_midi = 0;
    uint8_t test_count = 0;
    uint8_t *test_data;
    uint8_t test_bank = 0;
    uint8_t test_patch = 0;
    static char spinner[] = "|/-\\";
    static int spinpoint = 0;
    unsigned long int seek_to_sample;
    int inpause = 0;
    char * ret_err = NULL;
    long libraryver;
    char * lyric = NULL;
    char *last_lyric = NULL;
    uint32_t last_lyric_length = 0;
    int8_t kareoke = 0;
#define MAX_LYRIC_CHAR 128
    char lyrics[MAX_LYRIC_CHAR + 1];
#define MAX_DISPLAY_LYRICS 29
    char display_lyrics[MAX_DISPLAY_LYRICS + 1];

    memset(lyrics,' ',MAX_LYRIC_CHAR);
    memset(display_lyrics,' ',MAX_DISPLAY_LYRICS);

    config_file = "/freepats/freepats.cfg";

    // do_version();

    printf("Initializing Sound System\n");
    if (wav_file[0] != '\0') {
        if (open_wav_output(wav_file) == -1) {
            printf("Cannot open wave");
            completeConversion(1);
            return (1);
        }
    } else if (open_audio_output() == -1) {
        printf("Cannot audio output");
        completeConversion(1);
        return (1);
    }

    libraryver = WildMidi_GetVersion();
    printf("Initializing libWildMidi %ld.%ld.%ld\n\n",
                        (libraryver>>16) & 255,
                        (libraryver>> 8) & 255,
                        (libraryver    ) & 255);
    if (WildMidi_Init(config_file, rate, mixer_options) == -1) {
        printf("Cannot WildMidi_Init");
        completeConversion(1);
        return (1);
    }

    // printf(" +  Volume up        e  Better resampling    n  Next Midi\n");
    // printf(" -  Volume down      l  Log volume           q  Quit\n");
    // printf(" ,  1sec Seek Back   r  Reverb               .  1sec Seek Forward\n");
    // printf(" m  save as midi     p  Pause On/Off\n\n");

    output_buffer = malloc(16384);
    if (output_buffer == NULL) {
        fprintf(stderr, "Not enough memory, exiting\n");
        WildMidi_Shutdown();
        completeConversion(1);
        return (1);
    }

    WildMidi_MasterVolume(master_volume);

    // while (optind < argc || test_midi) {
        WildMidi_ClearError();
        if (!test_midi) {
            char *real_file = midi_file;
            printf("\rProcessing %s ", real_file);


            midi_ptr = WildMidi_Open(real_file);
            optind++;
            if (midi_ptr == NULL) {
                ret_err = WildMidi_GetError();
                printf(" Error opening midi: %s\r\n",ret_err);
            }
        }
        wm_info = WildMidi_GetInfo(midi_ptr);

        apr_mins = wm_info->approx_total_samples / (rate * 60);
        apr_secs = (wm_info->approx_total_samples % (rate * 60)) / rate;
        mixer_options = wm_info->mixer_options;
        modes[0] = (mixer_options & WM_MO_LOG_VOLUME)? 'l' : ' ';
        modes[1] = (mixer_options & WM_MO_REVERB)? 'r' : ' ';
        modes[2] = (mixer_options & WM_MO_ENHANCED_RESAMPLING)? 'e' : ' ';
        modes[3] = ' ';
        modes[4] = '\0';

        printf("\r\n[Duration of midi approx %2um %2us Total]\r\n", apr_mins, apr_secs);
        fprintf(stderr, "\r");

        memset(lyrics,' ',MAX_LYRIC_CHAR);
        memset(display_lyrics,' ',MAX_DISPLAY_LYRICS);

        while (1) {
            count_diff = wm_info->approx_total_samples
                        - wm_info->current_sample;

            if (count_diff == 0)
                break;

            ch = 0;

            if (inpause) {
                wm_info = WildMidi_GetInfo(midi_ptr);
                perc_play = (wm_info->current_sample * 100)
                            / wm_info->approx_total_samples;
                pro_mins = wm_info->current_sample / (rate * 60);
                pro_secs = (wm_info->current_sample % (rate * 60)) / rate;
                fprintf(stderr,
                        "%s [%s] [%3i] [%2um %2us Processed] [%2u%%] P  \r",
                        display_lyrics, modes, master_volume, pro_mins,
                        pro_secs, perc_play);
                msleep(5);
                continue;
            }

            res = WildMidi_GetOutput(midi_ptr, output_buffer,
                                     (count_diff >= 4096)? 16384 : (count_diff * 4));
            if (res <= 0)
                break;

            wm_info = WildMidi_GetInfo(midi_ptr);
            lyric = WildMidi_GetLyric(midi_ptr);

            memcpy(lyrics, &lyrics[1], MAX_LYRIC_CHAR - 1);
            lyrics[MAX_LYRIC_CHAR - 1] = ' ';

            if ((lyric != NULL) && (lyric != last_lyric) && (kareoke)) {
                last_lyric = lyric;
                if (last_lyric_length != 0) {
                    memcpy(lyrics, &lyrics[last_lyric_length], MAX_LYRIC_CHAR - last_lyric_length);
                }
                memcpy(&lyrics[MAX_DISPLAY_LYRICS], lyric, strlen(lyric));
                last_lyric_length = strlen(lyric);
            } else {
                if (last_lyric_length != 0) last_lyric_length--;
            }

            memcpy(display_lyrics,lyrics,MAX_DISPLAY_LYRICS);
            display_lyrics[MAX_DISPLAY_LYRICS] = '\0';

            perc_play = (wm_info->current_sample * 100)
                        / wm_info->approx_total_samples;
            pro_mins = wm_info->current_sample / (rate * 60);
            pro_secs = (wm_info->current_sample % (rate * 60)) / rate;
            // comment to prevent flooding console
            // fprintf(stderr,
            //     "%s [%s] [%3i] [%2um %2us Processed] [%2u%%] %c  \r",
            //     display_lyrics, modes, master_volume, pro_mins,
            //     pro_secs, perc_play, spinner[spinpoint++ % 4]);

            if (send_output(output_buffer, res) < 0) {
                /* driver prints an error message already. */
                printf("\r");
                goto end2;
            }

            // this converts to setTimeout and lets browser breath!
            if (sleep > -1) emscripten_sleep(sleep);
            // msleep(5);
            // end while
        }

        // NEXT MIDI
        // fprintf(stderr, "\r\n");
        if (WildMidi_Close(midi_ptr) == -1) {
            ret_err = WildMidi_GetError();
            fprintf(stderr, "OOPS: failed closing midi handle!\r\n%s\r\n",ret_err);
        }
        memset(output_buffer, 0, 16384);
        send_output(output_buffer, 16384);
    // }
    // end1:
    // memset(output_buffer, 0, 16384);
    // send_output(output_buffer, 16384);
    // msleep(5);

    end2:
    close_output();
    free(output_buffer);
    if (WildMidi_Shutdown() == -1) {
        ret_err = WildMidi_GetError();
        fprintf(stderr, "OOPS: failure shutting down libWildMidi\r\n%s\r\n", ret_err);
        WildMidi_ClearError();
    }

    printf("ok \r\n");
    completeConversion(0);

    // int x = EM_ASM_INT({
    //     console.log('take these values and do something', $0);
    //     // pass JS values back to C
    //     return $0;
    // }, some_c_values());

    return 0;
}

static void completeConversion(int status) {
    EM_ASM(
        completeConversion(status);
    );
}

// static int msleep(unsigned long milisec) {
//     emscripten_sleep(milisec);
//     return 1;
// }

/* helper / replacement functions: */

static int msleep(unsigned long milisec) {
    struct timespec req = { 0, 0 };
    time_t sec = (int) (milisec / 1000);
    milisec = milisec - (sec * 1000);
    req.tv_sec = sec;
    req.tv_nsec = milisec * 1000000L;
    while (nanosleep(&req, &req) == -1)
        continue;
    return (1);
}
