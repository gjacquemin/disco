#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
// 5 s of 1 kHz sine (-18 dBFS), then 2 s of digital silence, PCM16 mono.
// Same codec/I2S pipeline as WAV playback, no file reads or urn changes.
static void diagnostic_samples(int16_t *output,size_t count,uint32_t first) {
    for(size_t i=0;i<count;i++) {
        uint32_t n=first+i;
        output[i]=n<5U*44100U ? (int16_t)(4096.0f*sinf(6.28318530718f*(float)((n*1000U)%44100U)/44100.0f)) : 0;
    }
}
