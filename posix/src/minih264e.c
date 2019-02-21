#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#define MINIH264_IMPLEMENTATION
//#define MINIH264_ONLY_SIMD
#include "minih264e.h"

#define DEFAULT_GOP 20
#define DEFAULT_QP 33
#define DEFAULT_DENOISE 0

#define ENABLE_TEMPORAL_SCALABILITY 0
#define MAX_LONG_TERM_FRAMES        8 // used only if ENABLE_TEMPORAL_SCALABILITY==1

#define DEFAULT_MAX_FRAMES  99999

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

#if H264E_MAX_THREADS
#include "system.h"
typedef struct
{
    void *event_start;
    void *event_done;
    void (*callback)(void*);
    void *job;
    void *thread;
    int terminated;
} h264e_thread_t;

static THREAD_RET THRAPI minih264_thread_func(void *arg)
{
    h264e_thread_t *t = (h264e_thread_t *)arg;
    thread_name("h264");
    for (;;)
    {
        event_wait(t->event_start, INFINITE);
        if (t->terminated)
            break;
        t->callback(t->job);
        event_set(t->event_done);
    }
    return 0;
}

void *h264e_thread_pool_init(int max_threads)
{
    int i;
    h264e_thread_t *threads = (h264e_thread_t *)calloc(sizeof(h264e_thread_t), max_threads);
    if (!threads)
        return 0;
    for (i = 0; i < max_threads; i++)
    {
        h264e_thread_t *t = threads + i;
        t->event_start = event_create(0, 0);
        t->event_done  = event_create(0, 0);
        t->thread = thread_create(minih264_thread_func, t);
    }
    return threads;
}

void h264e_thread_pool_close(void *pool, int max_threads)
{
    int i;
    h264e_thread_t *threads = (h264e_thread_t *)pool;
    for (i = 0; i < max_threads; i++)
    {
        h264e_thread_t *t = threads + i;
        t->terminated = 1;
        event_set(t->event_start);
        thread_wait(t->thread);
        thread_close(t->thread);
        event_destroy(t->event_start);
        event_destroy(t->event_done);
    }
    free(pool);
}

void h264e_thread_pool_run(void *pool, void (*callback)(void*), void *callback_job[], int njobs)
{
    h264e_thread_t *threads = (h264e_thread_t*)pool;
    int i;
    for (i = 0; i < njobs; i++)
    {
        h264e_thread_t *t = threads + i;
        t->callback = (void (*)(void *))callback;
        t->job = callback_job[i];
        event_set(t->event_start);
    }
    for (i = 0; i < njobs; i++)
    {
        h264e_thread_t *t = threads + i;
        event_wait(t->event_done, INFINITE);
    }
}
#endif

struct
{
    const char *input_file;
    const char *output_file;
    int gen, gop, qp, kbps, max_frames, threads, speed, denoise, stats, psnr;
} cmdline[1];

static int str_equal(const char *pattern, char **p)
{
    if (!strncmp(pattern, *p, strlen(pattern)))
    {
        *p += strlen(pattern);
        return 1;
    } else
    {
        return 0;
    }
}

static int read_cmdline_options()
{
    int i;
    memset(cmdline, 0, sizeof(*cmdline));
    cmdline->gop = DEFAULT_GOP;
    cmdline->qp = DEFAULT_QP;
    cmdline->max_frames = DEFAULT_MAX_FRAMES;
    cmdline->kbps = 0;
    //cmdline->kbps = 2048;
    cmdline->denoise = DEFAULT_DENOISE;
    
    return 1;
}

typedef struct
{
    const char *size_name;
    int width;
    int h;
} frame_size_descriptor_t;

static const frame_size_descriptor_t g_frame_size_descriptor[] =
{
    {"sqcif",  128,   96},
    { "qvga",  320,  240},
    { "svga",  800,  600},
    { "4vga", 1280,  960},
    { "sxga", 1280, 1024},
    {  "xga", 1024,  768},
    {  "vga",  640,  480},
    { "qcif",  176,  144},
    { "4cif",  704,  576},
    { "4sif",  704,  480},
    {  "cif",  352,  288},
    {  "sif",  352,  240},
    {  "pal",  720,  576},
    { "ntsc",  720,  480},
    {   "d1",  720,  480},
    {"16cif", 1408, 1152},
    {"16sif", 1408,  960},
    { "720p", 1280,  720},
    {"4SVGA", 1600, 1200},
    { "4XGA", 2048, 1536},
    {"16VGA", 2560, 1920},
    {"16VGA", 2560, 1920},
    {NULL, 0, 0},
};

/**
*   Guess image size specification from ASCII string.
*   If string have several specs, only last one taken.
*   Spec may look like "352x288" or "qcif", "cif", etc.
*/
static int guess_format_from_name(const char *file_name, int *w, int *h)
{
    int i = (int)strlen(file_name);
    int found = 0;
    while(--i >= 0)
    {
        const frame_size_descriptor_t *fmt = g_frame_size_descriptor;
        const char *p = file_name + i;
        int prev_found = found;
        found = 0;
        if (*p >= '0' && *p <= '9')
        {
            char * end;
            int width = strtoul(p, &end, 10);
            if (width && (*end == 'x' || *end == 'X') && (end[1] >= '1' && end[1] <= '9'))
            {
                int height = strtoul(end + 1, &end, 10);
                if (height)
                {
                    *w = width;
                    *h = height;
                    found = 1;
                }
            }
        }
        do
        {
            if (!strncmp(file_name + i, fmt->size_name, strlen(fmt->size_name)))
            {
                *w = fmt->width;
                *h = fmt->h;
                found = 1;
            }
        } while((++fmt)->size_name);

        if (!found && prev_found)
        {
            return prev_found;
        }
    }
    return found;
}

// PSNR estimation results
typedef struct
{
    double psnr[4];             // PSNR, db
    double kpbs_30fps;          // bitrate, kbps, assuming 30 fps
    double psnr_to_logkbps_ratio;  // cumulative quality metric
    double psnr_to_kbps_ratio;  // another variant of cumulative quality metric
} rd_t;


static struct
{
    // Y,U,V,Y+U+V
    double noise[4];
    double count[4];
    double bytes;
    int frames;
} g_psnr;

static void psnr_init()
{
    memset(&g_psnr, 0, sizeof(g_psnr));
}

static void psnr_add(unsigned char *p0, unsigned char *p1, int w, int h, int bytes)
{
    int i, k;
    for (k = 0; k < 3; k++)
    {
        double s = 0;
        for (i = 0; i < w*h; i++)
        {
            int d = *p0++ - *p1++;
            s += d*d;
        }
        g_psnr.count[k] += w*h;
        g_psnr.noise[k] += s;
        if (!k) w >>= 1, h >>= 1;
    }
    g_psnr.count[3] = g_psnr.count[0] + g_psnr.count[1] + g_psnr.count[2];
    g_psnr.noise[3] = g_psnr.noise[0] + g_psnr.noise[1] + g_psnr.noise[2];
    g_psnr.frames++;
    g_psnr.bytes += bytes;
}

static rd_t psnr_get()
{
    int i;
    rd_t rd;
    double fps = 30;
    double realkbps = g_psnr.bytes*8./((double)g_psnr.frames/(fps))/1000;
    double db = 10*log10(255.*255/(g_psnr.noise[0]/g_psnr.count[0]));
    for (i = 0; i < 4; i++)
    {
        rd.psnr[i] = 10*log10(255.*255/(g_psnr.noise[i]/g_psnr.count[i]));
    }
    rd.psnr_to_kbps_ratio = 10*log10((double)g_psnr.count[0]*g_psnr.count[0]*3/2 * 255*255/(g_psnr.noise[0] * g_psnr.bytes));
    rd.psnr_to_logkbps_ratio = db / log10(realkbps);
    rd.kpbs_30fps = realkbps;
    return rd;
}

static void psnr_print(rd_t rd)
{
    int i;
    printf("%5.0f kbps@30fps  ", rd.kpbs_30fps);
    for (i = 0; i < 3; i++)
    {
        //printf("  %.2f db ", rd.psnr[i]);
        printf(" %s=%.2f db ", i ? (i == 1 ? "UPSNR" : "VPSNR") : "YPSNR", rd.psnr[i]);
    }
    printf("  %6.2f db/rate ", rd.psnr_to_kbps_ratio);
    printf("  %6.3f db/lgrate ", rd.psnr_to_logkbps_ratio);
    printf("  \n");
}

static int pixel_of_chessboard(double x, double y)
{
#if 0
    int mid = (fabs(x) < 4 && fabs(y) < 4);
    int i = (int)(x);
    int j = (int)(y);
    int cx, cy;
    cx = (i & 16) ? 255 : 0;
    cy = (j & 16) ? 255 : 0;
    if ((i & 15) == 0) cx *= (x - i);
    if ((j & 15) == 0) cx *= (y - j);
    return (cx + cy + 1) >> 1;
#else
    int mid = (fabs(x ) < 4 && fabs(y) < 4);
    int i = (int)(x);
    int j = (int)(y);
    int black = (mid) ? 128 : i/16;
    int white = (mid) ? 128 : 255 - j/16;
    int c00 = (((i >> 4) + (j >> 4)) & 1) ? white : black;
    int c01 = ((((i + 1)>> 4) + (j >> 4)) & 1) ? white : black;
    int c10 = (((i >> 4) + ((j + 1) >> 4)) & 1) ? white : black;
    int c11 = ((((i + 1) >> 4) + ((j + 1) >> 4)) & 1) ? white : black;
    int s    = (int)((c00 * (1 - (x - i)) + c01*(x - i))*(1 - (y - j)) +
                     (c10 * (1 - (x - i)) + c11*(x - i))*((y - j)) + 0.5);
    return s < 0 ? 0 : s > 255 ? 255 : s;
#endif
}

static void gen_chessboard_rot(unsigned char *p, int w, int h, int frm)
{
    int r, c;
    double x, y;
    double co = cos(.01*frm);
    double si = sin(.01*frm);
    int hw = w >> 1;
    int hh = h >> 1;
    for (r = 0; r < h; r++)
    {
        for (c = 0; c < w; c++)
        {
            x =  co*(c - hw) + si*(r - hh);
            y = -si*(c - hw) + co*(r - hh);
            p[r*w + c] = pixel_of_chessboard(x, y);
        }
    }
}

int encode(int width, int height, FILE *fin, FILE *fout)
{
    int i, frames = 0;
    const char *fnin, *fnout;
    uint8_t *buf_in;

    if (!read_cmdline_options())
        return 1;
    fnin  = "out.yuv";
    fnout = "out.264";

    if (!fnout)
        fnout = "out.264";

    create_param.enableNEON = 1;
#if H264E_SVC_API
    create_param.num_layers = 1;
    create_param.inter_layer_pred_flag = 1;
    create_param.inter_layer_pred_flag = 0;
#endif
    create_param.gop = cmdline->gop;
    create_param.height = height;
    create_param.width  = width;
    create_param.max_long_term_reference_frames = 0;
#if ENABLE_TEMPORAL_SCALABILITY
    create_param.max_long_term_reference_frames = MAX_LONG_TERM_FRAMES;
#endif
    create_param.fine_rate_control_flag = 0;
    create_param.const_input_flag = cmdline->psnr ? 0 : 1;
    //create_param.vbv_overflow_empty_frame_flag = 1;
    //create_param.vbv_underflow_stuffing_flag = 1;
    create_param.vbv_size_bytes = 100000/8;
    create_param.temporal_denoise_flag = cmdline->denoise;
    //create_param.vbv_size_bytes = 1500000/8;

#if H264E_MAX_THREADS
    void *thread_pool = NULL;
    create_param.max_threads = cmdline->threads;
    if (cmdline->threads)
    {
        thread_pool = h264e_thread_pool_init(cmdline->threads);
        create_param.token = thread_pool;
        create_param.run_func_in_thread = h264e_thread_pool_run;
    }
#endif

    frame_size = width*height*3/2;
    buf_in   = (uint8_t*)ALIGNED_ALLOC(64, frame_size);
    // buf_save = (uint8_t*)ALIGNED_ALLOC(64, frame_size);

    // if (!buf_in || !buf_save)
    // {
    //     printf("ERROR: not enough memory\n");
    //     return 1;
    // }
    //for (cmdline->qp = 10; cmdline->qp <= 51; cmdline->qp += 10)
    //for (cmdline->qp = 40; cmdline->qp <= 51; cmdline->qp += 10)
    //for (cmdline->qp = 50; cmdline->qp <= 51; cmdline->qp += 2)
    //printf("encoding %s to %s with qp = %d\n", fnin, fnout, cmdline->qp);
    {
        int sum_bytes = 0;
        int max_bytes = 0;
        int min_bytes = 10000000;
        int sizeof_persist = 0, sizeof_scratch = 0, error;
        H264E_persist_t *enc = NULL;
        H264E_scratch_t *scratch = NULL;
        if (cmdline->psnr)
            psnr_init();

        error = H264E_sizeof(&create_param, &sizeof_persist, &sizeof_scratch);
        if (error)
        {
            printf("H264E_init error = %d\n", error);
            return 0;
        }
        printf("sizeof_persist = %d sizeof_scratch = %d\n", sizeof_persist, sizeof_scratch);
        enc     = (H264E_persist_t *)ALIGNED_ALLOC(64, sizeof_persist);
        scratch = (H264E_scratch_t *)ALIGNED_ALLOC(64, sizeof_scratch);
        error = H264E_init(enc, &create_param);
        
        for (i = 0; cmdline->max_frames; i++)
        {
            // if (!fin)
            // {
            //     if (i > 300) break;
            //     memset(buf_in + width*height, 128, width*height/2);
            //     gen_chessboard_rot(buf_in, width, height, i);
            // } else
            //     if (!fread(buf_in, frame_size, 1, fin)) break;
            // if (cmdline->psnr)
            //     memcpy(buf_save, buf_in, frame_size);
            if (!fread(buf_in, frame_size, 1, fin)) break;
            yuv.yuv[0] = buf_in; yuv.stride[0] = width;
            yuv.yuv[1] = buf_in + width*height; yuv.stride[1] = width/2;
            yuv.yuv[2] = buf_in + width*height*5/4; yuv.stride[2] = width/2;

            run_param.frame_type = 0;
            run_param.encode_speed = cmdline->speed;
            //run_param.desired_nalu_bytes = 100;

            if (cmdline->kbps)
            {
                run_param.desired_frame_bytes = cmdline->kbps*1000/8/30;
                run_param.qp_min = 10;
                run_param.qp_max = 50;
            } else
            {
                run_param.qp_min = run_param.qp_max = cmdline->qp;
            }

#if ENABLE_TEMPORAL_SCALABILITY
            {
            int level, logmod = 1;
            int j, mod = 1 << logmod;
            static int fresh[200] = {-1,-1,-1,-1};

            run_param.frame_type = H264E_FRAME_TYPE_CUSTOM;

            for (level = logmod; level && (~i & (mod >> level)); level--){}

            run_param.long_term_idx_update = level + 1;
            if (level == logmod && logmod > 0)
                run_param.long_term_idx_update = -1;
            if (level == logmod - 1 && logmod > 1)
                run_param.long_term_idx_update = 0;

            //if (run_param.long_term_idx_update > logmod) run_param.long_term_idx_update -= logmod+1;
            //run_param.long_term_idx_update = logmod - 0 - level;
            //if (run_param.long_term_idx_update > 0)
            //{
            //    run_param.long_term_idx_update = logmod - run_param.long_term_idx_update;
            //}
            run_param.long_term_idx_use    = fresh[level];
            for (j = level; j <= logmod; j++)
            {
                fresh[j] = run_param.long_term_idx_update;
            }
            if (!i)
            {
                run_param.long_term_idx_use = -1;
            }
            }
#endif
            error = H264E_encode(enc, scratch, &run_param, &yuv, &coded_data, &sizeof_coded_data);
            assert(!error);

            if (i)
            {
                sum_bytes += sizeof_coded_data - 4;
                if (min_bytes > sizeof_coded_data - 4) min_bytes = sizeof_coded_data - 4;
                if (max_bytes < sizeof_coded_data - 4) max_bytes = sizeof_coded_data - 4;
            }

            // if (cmdline->stats)
                printf("frame=%d, bytes=%d\n", frames++, sizeof_coded_data);

            if (fout)
            {
                if (!fwrite(coded_data, sizeof_coded_data, 1, fout))
                {
                    printf("ERROR writing output file\n");
                    // break;
                }
            }
            // if (cmdline->psnr)
                // psnr_add(buf_save, buf_in, width, height, sizeof_coded_data);
        }
        //fprintf(stderr, "%d avr = %6d  [%6d %6d]\n", qp, sum_bytes/299, min_bytes, max_bytes);

        // if (cmdline->psnr)
            // psnr_print(psnr_get());
        free(buf_in);
        if (enc)
            free(enc);
        if (scratch)
            free(scratch);
    }

#if H264E_MAX_THREADS
    if (thread_pool)
    {
        h264e_thread_pool_close(thread_pool, cmdline->threads);
    }
#endif
    return 0;
}
