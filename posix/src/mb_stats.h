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
} mb_stats_t;

void initMbStats(mb_stats_t* mbStats);
void destroyMbStats(mb_stats_t* mbStats);
void showMbStats(mb_stats_t* mbStats);
void addMbType(int mbType, mb_stats_t* mbStats);
#endif