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
#include "src/minih264e.c"

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

void YUV_read_and_show(u8* picData, int width, int height, int picNum) {
  IplImage *bgr;
  bgr = cvCreateImage(cvSize(width,height), IPL_DEPTH_8U, 3);
  // Init
  IplImage *ycrcb = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 3);
  IplImage *y = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 1);
  IplImage *cb = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 1);
  IplImage *cr = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 1);
  IplImage *cb_half = cvCreateImage(cvSize(width/2, height/2), IPL_DEPTH_8U, 1);
  IplImage *cr_half = cvCreateImage(cvSize(width/2, height/2), IPL_DEPTH_8U, 1);
  
  // Decode and show
  size_t bytes_read;
  size_t npixels;
  
  npixels = width*height;
  int current = 0;
  for (int i = current; i < npixels * sizeof(uint8_t); i++) {
    *(y->imageData + i) = *(picData + i);
    current++;
  }
  
  for (int i = 0; i < npixels * sizeof(uint8_t) / 4; i++) {
    *(cb_half->imageData + i) = *(picData + current + i);
  }

  for (int i = 0; i < npixels * sizeof(uint8_t) / 4; i++) {
    *(cr_half->imageData + i) = *(picData + current + i);
  }

  cvResize(cb_half, cb, CV_INTER_CUBIC);
  cvResize(cr_half, cr, CV_INTER_CUBIC);
  cvMerge(y, cr, cb, NULL, ycrcb);
  cvCvtColor(ycrcb, bgr, CV_YCrCb2BGR);
  cvShowImage("frame", bgr);
  cvWaitKey(10);
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
  fout = NULL;
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
          encode(width, height, pic, fout);
        }
        if (comparePath) totalErrors += comparePics(pic, width, height, numPics);
        YUV_read_and_show(pic, width, height, numPics);
        break;
      case H264BSD_HDRS_RDY:
        h264bsdCroppingParams(&dec, &croppingFlag, &left, &width, &top, &height);
        if (!croppingFlag) {
          width = h264bsdPicWidth(&dec) * 16;
          height = h264bsdPicHeight(&dec) * 16;
        }

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
  fclose(fout);

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