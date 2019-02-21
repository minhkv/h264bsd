#include <stdio.h>
#include <stdint.h>
#include <cv.h>

#include "showImage.h"


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
  size_t nbytes = width*height * sizeof(uint8_t);
  int current = 0;

  memcpy(y->imageData, picData, nbytes);
  memcpy(cb_half->imageData, picData + nbytes, nbytes / 4);
  memcpy(cr_half->imageData, picData + 5 * nbytes / 4, nbytes / 4);

  cvResize(cb_half, cb, CV_INTER_CUBIC);
  cvResize(cr_half, cr, CV_INTER_CUBIC);
  cvMerge(y, cr, cb, NULL, ycrcb);
  cvCvtColor(ycrcb, bgr, CV_YCrCb2BGR);
  cvShowImage("frame", bgr);
  cvReleaseImage(&ycrcb);
  cvReleaseImage(&y);
  cvReleaseImage(&cb);
  cvReleaseImage(&cr);
  cvReleaseImage(&cb_half);
  cvReleaseImage(&cr_half);
  cvReleaseImage(&bgr);
  cvWaitKey(10);
}
