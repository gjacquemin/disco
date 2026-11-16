#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
// Validated PCM16 frames, distinct input/output buffers; output capacity >= frames*2.
static size_t pcm_to_stereo(const int16_t *input,int16_t *output,size_t frames,unsigned channels,bool mute) {
    size_t bytes=frames*2*sizeof(int16_t);
    if(mute) memset(output,0,bytes);
    else if(channels==2) memcpy(output,input,bytes); // interleaved left, right
    else for(size_t i=0;i<frames;i++) output[2*i]=output[2*i+1]=input[i];
    return bytes;
}
