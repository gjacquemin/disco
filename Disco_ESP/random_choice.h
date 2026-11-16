#pragma once
#include <stdint.h>
/* n>0 ; skip=-1 ou index a exclure, uniquement si n>1. */
static uint32_t choose_index(uint32_t n,int skip,uint32_t (*rng)(void)) {
    uint32_t choices=n-(skip>=0 && n>1);
    uint32_t threshold=(uint32_t)(-choices)%choices,r;
    do { r=rng(); } while(r<threshold);
    r%=choices;
    if(skip>=0 && n>1 && r>=(uint32_t)skip) r++;
    return r;
}
