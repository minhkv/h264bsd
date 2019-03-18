#ifndef MB_STATS_H
#define MB_STATS_H

#include <stdio.h>
#include <stdlib.h>
typedef struct {
    int skip;
    int p16x16;
    int p16x8;
    int p8x8;
    int i4x4;
    int i16x16;

    int skip_bytes;
    int p16x16_bytes;
    int p16x8_bytes;
    int p8x8_bytes;
    int i4x4_bytes;
    int i16x16_bytes;
} mb_stats_t;

void initMbStats(mb_stats_t* mbStats);
void destroyMbStats(mb_stats_t* mbStats);
void showMbStats(mb_stats_t* mbStats);
void addMbType(int mbType, int bytes, mb_stats_t* mbStats);
#endif