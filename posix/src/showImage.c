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

void YUV_read_and_show1(image_t* picData, int width, int height, int picNum) {
  IplImage *bgr;
  
  int w = width;
  int h = height;
  // width = picData->width;
  // height = picData->height;
  // Init
  bgr = cvCreateImage(cvSize(w,h), IPL_DEPTH_8U, 3);
  IplImage *ycrcb_resize = cvCreateImage(cvSize(w, h), IPL_DEPTH_8U, 3);
  IplImage *ycrcb = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 3);
  IplImage *y = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 1);
  IplImage *cb = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 1);
  IplImage *cr = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 1);
  IplImage *cb_half = cvCreateImage(cvSize(width/2, height/2), IPL_DEPTH_8U, 1);
  IplImage *cr_half = cvCreateImage(cvSize(width/2, height/2), IPL_DEPTH_8U, 1);
  u8 *pLum, *pCb, *pCr;
  // Decode and show
  size_t bytes_read;
  size_t npixels;

  pLum = (u8*)picData->luma;
  pCb = (u8*)picData->cb;
  pCr = (u8*)picData->cr;
  
  npixels = width*height;
  for (int i = 0; i < npixels * sizeof(u8); i++) {
    *(y->imageData + i) = *(pLum + i);
  }
  
  for (int i = 0; i < npixels * sizeof(u8) / 4; i++) {
    *(cb_half->imageData + i) = *(pCb + i);
  }

  for (int i = 0; i < npixels * sizeof(u8) / 4; i++) {
    *(cr_half->imageData + i) = *(pCr + i);
  }

  cvResize(cb_half, cb, CV_INTER_CUBIC);
  cvResize(cr_half, cr, CV_INTER_CUBIC);
  cvMerge(y, cr, cb, NULL, ycrcb);
  cvResize(ycrcb, ycrcb_resize, CV_INTER_CUBIC);
  cvCvtColor(ycrcb_resize, bgr, CV_YCrCb2BGR);
  cvShowImage("frame", bgr);

  cvReleaseImage(&ycrcb_resize);
  cvReleaseImage(&ycrcb);
  cvReleaseImage(&y);
  cvReleaseImage(&cb);
  cvReleaseImage(&cr);
  cvReleaseImage(&cb_half);
  cvReleaseImage(&cr_half);
  cvReleaseImage(&bgr);
  cvWaitKey(0);
}
