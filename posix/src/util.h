#ifndef UTIL_H
#define UTIL_H

struct
{
    const char *input_file;
    const char *output_file;
    int gen, gop, qp, kbps, max_frames, threads, speed, denoise, stats, psnr;
    int debug;
} cmdline[1];

#endif