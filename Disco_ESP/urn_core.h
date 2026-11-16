#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "random_choice.h"
#define URN_MAX 1024
static void urn_shuffle(uint16_t *order, unsigned n, uint32_t (*rng)(void)) {
    for(unsigned i=0;i<n;i++) order[i]=i;
    for(unsigned i=n;i>1;i--) {
        unsigned j=choose_index(i,-1,rng);
        uint16_t t=order[i-1]; order[i-1]=order[j]; order[j]=t;
    }
}
static bool urn_valid(const uint16_t *order,unsigned n,unsigned cursor) {
    if(n>URN_MAX || cursor>n) return false;
    uint8_t seen[URN_MAX/8]={0};
    for(unsigned i=0;i<n;i++) {
        unsigned v=order[i];
        if(v>=n || (seen[v/8]&(1U<<(v%8)))) return false;
        seen[v/8]|=1U<<(v%8);
    }
    return true;
}
