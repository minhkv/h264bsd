#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#define MINIH264_IMPLEMENTATION
//#define MINIH264_ONLY_SIMD
#include "minih264e.h"
#include "h264bsd_decoder.h"
#define DEFAULT_GOP 20
#define DEFAULT_QP 30
#define DEFAULT_DENOISE 0

#define ENABLE_TEMPORAL_SCALABILITY 0
#define MAX_LONG_TERM_FRAMES        8 // used only if ENABLE_TEMPORAL_SCALABILITY==1

#define DEFAULT_MAX_FRAMES  100

H264E_create_param_t create_param;
H264E_run_param_t run_param;
H264E_io_yuv_t yuv;
uint8_t *buf_in, *buf_save;
uint8_t *coded_data;
FILE *fin, *fout;
int sizeof_coded_data, frame_size, width, height, _qp;

#ifdef _WIN32
// only vs2017 have aligned_alloc
#define ALIGNED_ALLOC(n, size) malloc(size)
#else
#define ALIGNED_ALLOC(n, size) aligned_alloc(n, size)
#endif


struct
{
    const char *input_file;
    const char *output_file;
    int gen, gop, qp, kbps, max_frames, threads, speed, denoise, stats, psnr;
} cmdline[1];


int encode(int width, int height, uint8_t *buf_in, FILE *fout)
{
    int i, frames = 0;
    const char *fnin, *fnout;

    if(fout == NULL) {
        fout = fopen("out.264", "wb");
    }

    memset(cmdline, 0, sizeof(*cmdline));
    cmdline->gop = DEFAULT_GOP;
    cmdline->qp = DEFAULT_QP;
    cmdline->max_frames = DEFAULT_MAX_FRAMES;
    cmdline->kbps = 0;
    cmdline->denoise = DEFAULT_DENOISE;

    create_param.enableNEON = 1;
    create_param.num_layers = 1;
    create_param.inter_layer_pred_flag = 1;
    create_param.inter_layer_pred_flag = 0;
    create_param.gop = cmdline->gop;
    create_param.height = height;
    create_param.width  = width;
    create_param.max_long_term_reference_frames = 0;
    create_param.fine_rate_control_flag = 0;
    create_param.const_input_flag = cmdline->psnr ? 0 : 1;
    create_param.vbv_size_bytes = 100000/8;
    create_param.temporal_denoise_flag = cmdline->denoise;

    frame_size = width*height*3/2;

    int sizeof_persist = 0, sizeof_scratch = 0, error;
    H264E_persist_t *enc = NULL;
    H264E_scratch_t *scratch = NULL;

    error = H264E_sizeof(&create_param, &sizeof_persist, &sizeof_scratch);
    enc     = (H264E_persist_t *)ALIGNED_ALLOC(64, sizeof_persist);
    scratch = (H264E_scratch_t *)ALIGNED_ALLOC(64, sizeof_scratch);
    error = H264E_init(enc, &create_param);

    yuv.yuv[0] = buf_in; yuv.stride[0] = width;
    yuv.yuv[1] = buf_in + width*height; yuv.stride[1] = width/2;
    yuv.yuv[2] = buf_in + width*height*5/4; yuv.stride[2] = width/2;

    run_param.frame_type = 0;
    run_param.encode_speed = cmdline->speed;

    run_param.qp_min = run_param.qp_max = cmdline->qp;

    error = H264E_encode(enc, scratch, &run_param, &yuv, &coded_data, &sizeof_coded_data);
    assert(!error);

    if (!fwrite(coded_data, sizeof_coded_data, 1, fout))
    {
        printf("ERROR writing output file\n");
    }

    if (enc)
        free(enc);
    if (scratch)
        free(scratch);

    return 0;
}
