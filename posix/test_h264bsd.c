/* Small application used to test teh h264bsd library on a posix compatible syste */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include "src/h264bsd_decoder.h"
#include "src/h264bsd_util.h"

#include <stdlib.h>
#include <assert.h>
#include <highgui.h>

#include "src/h264bsd_decoder.h"
#include "src/h264bsd_util.h"
#include "yuv.h"
#include "src/minih264e.h"
#include "encoder.c"
#include "src/showImage.h"
#include "src/showImage.c"

static char* outputPath = NULL;
static char* comparePath = NULL;
static int repeatTest = 0;

void createContentBuffer(char* contentPath, u8** pContentBuffer, size_t* pContentSize) {
  struct stat sb;
  if (stat(contentPath, &sb) == -1) {
    perror("stat failed");
    exit(1);
  }

  *pContentSize = sb.st_size;
  *pContentBuffer = (u8*)malloc(*pContentSize);
}

void loadContent(char* contentPath, u8* contentBuffer, size_t contentSize) {
  FILE *input = fopen(contentPath, "r");
  if (input == NULL) {
    perror("open failed");
    exit(1);
  }

  off_t offset = 0;
  while (offset < contentSize) {
    offset += fread(contentBuffer + offset, sizeof(u8), contentSize - offset, input);
  }

  fclose(input);
}

static FILE *outputFile = NULL;

void savePic(u8* picData, int width, int height, int picNum) {
  if(outputFile == NULL) {
    outputFile = fopen(outputPath, "w");
    if (outputFile == NULL) {
      perror("output file open failed");
      exit(1);
    }
  }

  size_t picSize = width * height * 3 / 2;
  off_t offset = 0;
  while (offset < picSize) {
    offset += fwrite(picData + offset, sizeof(u8), picSize - offset, outputFile);
  }
}


static FILE *compareFile = NULL;
static u8* expectedData = NULL;
static int totalErrors = 0;

int comparePics(u8* actualData, int width, int height, int picNum) {
  if(compareFile == NULL) {
    compareFile = fopen(comparePath, "r");
    if (compareFile == NULL) {
      perror("compare file open failed");
      exit(1);
    }
  }

  size_t picSize = width * height * 3 / 2;
  size_t uDataOffset = width * height;
  size_t vDataOffset = width * height * 5 / 4;

  if (!expectedData) expectedData = (u8*)malloc(picSize);

  off_t offset = 0;
  while (offset < picSize) {
    offset += fread(expectedData + offset, sizeof(u8), picSize - offset, compareFile);
  }

  int numErrors = 0;

  size_t yOffset = 0;
  size_t uvOffset = 0;

  u8* yExpected = expectedData;
  u8* uExpected = expectedData + uDataOffset;
  u8* vExpected = expectedData + vDataOffset;

  u8* yActual = actualData;
  u8* uActual = actualData + uDataOffset;
  u8* vActual = actualData + vDataOffset;

  for (int y=0; y<height; ++y) {
    for (int x=0; x<width; ++x) {
      int ySame = yActual[yOffset] == yExpected[yOffset];
      int uSame = uActual[uvOffset] == uExpected[uvOffset];
      int vSame = vActual[uvOffset] == vExpected[uvOffset];

      if(!ySame || !uSame || !vSame) {
        ++numErrors;
        if (numErrors <= 5) {
          printf(
            "Pixel (%d,%d) is different. Expected (%d,%d,%d) but saw (%d,%d,%d).\n",
            x, y,
            yExpected[yOffset], uExpected[uvOffset], vExpected[uvOffset],
            yActual[yOffset], uActual[uvOffset], vActual[uvOffset]);
        }

        if (numErrors == 6) printf("...\n");
      }

      ++yOffset;
      if(yOffset % 1) ++ uvOffset;
    }
  }

  if(numErrors > 0) printf("%d pixels are different on frame %d.\n\n", numErrors, picNum);
  return numErrors;
}

static H264E_persist_t *enc = NULL;
static H264E_scratch_t *scratch = NULL;
static int sizeof_persist = 0, sizeof_scratch = 0, error;

void init_encode(int width, int height) {
  read_cmdline_options();

  create_param.enableNEON = 1;
#if H264E_SVC_API
  create_param.num_layers = 1;
  create_param.inter_layer_pred_flag = 1;
  create_param.inter_layer_pred_flag = 0;
#endif
  create_param.fine_rate_control_flag = 0;
    create_param.const_input_flag = cmdline->psnr ? 0 : 1;
    //create_param.vbv_overflow_empty_frame_flag = 1;
    //create_param.vbv_underflow_stuffing_flag = 1;
    create_param.vbv_size_bytes = 100000/8;
    create_param.temporal_denoise_flag = cmdline->denoise;
    //create_param.vbv_size_bytes = 1500000/8;

  create_param.gop = cmdline->gop;
  create_param.height = height;
  create_param.width = width;
  create_param.max_long_term_reference_frames = 0;

  // int sizeof_persist = 0, sizeof_scratch = 0, error;
  error = H264E_sizeof(&create_param, &sizeof_persist, &sizeof_scratch);
        if (error)
        {
            printf("H264E_init error = %d\n", error);
            return 0;
        }
  printf("sizeof_persist = %d sizeof_scratch = %d\n", sizeof_persist, sizeof_scratch);

  if (enc == NULL)
          enc     = (H264E_persist_t *)ALIGNED_ALLOC(64, sizeof_persist);
        if (scratch == NULL)
          scratch = (H264E_scratch_t *)ALIGNED_ALLOC(64, sizeof_scratch);
        error = H264E_init(enc, &create_param);
        
} 

int encode(int width, int height, FILE *fin, FILE *fout, storage_t dec, int numPics)
{
    int i, frames = 0;
    const char *fnin, *fnout;
    uint8_t *buf_in;

    
    fnin  = "out.yuv";
    fnout = "out.264";

    if (!fnout)
        fnout = "out.264";

  


#if ENABLE_TEMPORAL_SCALABILITY
    create_param.max_long_term_reference_frames = MAX_LONG_TERM_FRAMES;
#endif
    

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
    // buf_in   = (uint8_t*)ALIGNED_ALLOC(64, frame_size);
    buf_in = dec.currImage->data;
    // buf_save = (uint8_t*)ALIGNED_ALLOC(64, frame_size);


    {
        int sum_bytes = 0;
        int max_bytes = 0;
        int min_bytes = 10000000;
        // int sizeof_persist = 0, sizeof_scratch = 0, error;

        if (cmdline->psnr)
            psnr_init();

        
        
        // enc->frame.num = numPics;
        frames = numPics;
        // for (i = 0; cmdline->max_frames; i++)
        {

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
            error = H264E_encode(enc, scratch, &run_param, &yuv, &coded_data, &sizeof_coded_data, dec, numPics);
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
        // free(buf_in);
        
    }

#if H264E_MAX_THREADS
    if (thread_pool)
    {
        h264e_thread_pool_close(thread_pool, cmdline->threads);
    }
#endif
    return 0;
}


void decodeContent (u8* contentBuffer, size_t contentSize) {
  u32 status;
  storage_t dec;
  status = h264bsdInit(&dec, HANTRO_FALSE);
  
  if (status != HANTRO_OK) {
    fprintf(stderr, "h264bsdInit failed\n");
    exit(1);
  }
  

  u8* byteStrm = contentBuffer;
  u32 readBytes;
  u32 len = contentSize;
  int numPics = 0;
  u8* pic;
  u32 picId, isIdrPic, numErrMbs;
  u32 top, left, width, height, croppingFlag;
  int totalErrors = 0;
  FILE *fout = fopen("out.264", "wb");
  while (len > 0) {
    u32 result = h264bsdDecode(&dec, byteStrm, len, 0, &readBytes);
    len -= readBytes;
    byteStrm += readBytes;

    switch (result) {
      case H264BSD_PIC_RDY:
        pic = h264bsdNextOutputPicture(&dec, &picId, &isIdrPic, &numErrMbs);
        ++numPics;
        if (outputPath) {
          savePic(pic, width, height, numPics);
        }
        if (comparePath) totalErrors += comparePics(pic, width, height, numPics);
        encode(width, height, outputFile, fout, dec, numPics - 1);
        YUV_read_and_show(pic, width, height, numPics);
        break;
      case H264BSD_HDRS_RDY:
        h264bsdCroppingParams(&dec, &croppingFlag, &left, &width, &top, &height);
        // if (!croppingFlag) {
          width = h264bsdPicWidth(&dec) * 16;
          height = h264bsdPicHeight(&dec) * 16;
          init_encode(width, height);
        // }
        char* cropped = croppingFlag ? "(cropped) " : "";
        printf("Decoded headers. Image size %s%dx%d.\n", cropped, width, height);
        break;
      case H264BSD_RDY:
        break;
      case H264BSD_ERROR:
        printf("Error\n");
        exit(1);
      case H264BSD_PARAM_SET_ERROR:
        printf("Param set error\n");
        exit(1);
    }
  }
  // fclose(outputFile);
  // outputFile = fopen(outputPath, "r");
  // encode(width, height, outputFile, fout, dec);
  fclose(fout);
  if (enc)
      free(enc);
  if (scratch)
      free(scratch);
  h264bsdShutdown(&dec);

  printf("Test file complete. %d pictures decoded.\n", numPics);
  if (comparePath) printf("%d errors found.\n", totalErrors);
}

int main(int argc, char *argv[]) {
  int c;
  while ((c = getopt (argc, argv, "ro:c:")) != -1) {
    switch (c) {
      case 'o':
        outputPath = optarg;
        break;
      case 'c':
        comparePath = optarg;
        break;
      case 'r':
        repeatTest = 1;
        break;
      default:
        abort();
    }
  }

  if (argc < 2) {
    fprintf(stderr, "Usage: %s [-r] [-c <compare.yuv>] [-o <output.yuv>] <test_video.h264>\n", argv[0]);
    exit(1);
  }

  char *contentPath = argv[argc - 1];
  u8* contentBuffer;
  size_t contentSize;
  createContentBuffer(contentPath, &contentBuffer, &contentSize);

  if (repeatTest) {
    while (1) {
      loadContent(contentPath, contentBuffer, contentSize);
      decodeContent(contentBuffer, contentSize);
    }
  } else {
    loadContent(contentPath, contentBuffer, contentSize);
    decodeContent(contentBuffer, contentSize);
  }

  if(outputFile) fclose(outputFile);
  if(compareFile) fclose(compareFile);
}