#include "mb_stats.h"

void initMbStats(mb_stats_t *mbStats) {
    // mbStats = (mb_stats_t*) malloc(sizeof(mb_stats_t));
    mbStats->skip = 0;
    mbStats->p16x16 = 0;
    mbStats->p16x8 = 0;
    mbStats->p8x8 = 0;
    mbStats->i4x4 = 0;
    mbStats->i16x16 = 0;

    mbStats->skip_bytes = 0;
    mbStats->p16x16_bytes = 0;
    mbStats->p16x8_bytes = 0;
    mbStats->p8x8_bytes = 0;
    mbStats->i4x4_bytes = 0;
    mbStats->i16x16_bytes = 0;
}
void destroyMbStats(mb_stats_t *mbStats) {
    free(mbStats);
}

void showMbStats(mb_stats_t *mbStats) {
    printf("%6s: %3d %3d, ", "Skip", mbStats->skip, mbStats->skip_bytes);
    printf("%6s: %3d %3d, ", "P16x16", mbStats->p16x16, mbStats->p16x16_bytes);
    printf("%6s: %3d %3d, ", "P16x8", mbStats->p16x8, mbStats->p16x8_bytes);
    printf("%6s: %3d %3d, ", "P8x8", mbStats->p8x8, mbStats->p8x8_bytes);
    printf("%6s: %3d %3d, ", "I4x4", mbStats->i4x4, mbStats->i4x4_bytes);
    printf("%6s: %3d %3d\n", "I16x16", mbStats->i16x16, mbStats->i16x16_bytes);
    return;
}
void addMbType(int mbType, int bytes, mb_stats_t *mbStats) {
    switch(mbType) {
        case 0:
            mbStats->skip++;
            mbStats->skip_bytes += bytes;
            break;
        case 1:
            mbStats->p16x16++;
            mbStats->p16x16_bytes += bytes;
            break;
        case 2:
            mbStats->p16x8++;
            mbStats->p16x8_bytes += bytes;
            break;
        case 3:
            // mbStats->skip++;
            break;
        case 4:
            mbStats->p8x8++;
            mbStats->p8x8_bytes += bytes;
            break;
        case 5:
            // mbStats->skip++;
            break;
        case 6:
            mbStats->i4x4++;
            mbStats->i4x4_bytes += bytes;
            break;
        default:
            mbStats->i16x16++;
            mbStats->i16x16_bytes += bytes;
            break;
    }
}