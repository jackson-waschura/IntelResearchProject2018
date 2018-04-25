#pragma once
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/core/utils/trace.hpp>

using namespace cv;
using namespace cv::dnn;

#define _ATL_APARTMENT_THREADED

#include <atlbase.h>
extern CComModule _Module;

const int W = 640;
const int H = 480;
const int TARGET_RED = 0xC5;
const int TARGET_GREEN = 0x1B;
const int TARGET_BLUE = 0x77;
const int TARGET_DIST = 90;
const int TEMP_BUFFER_SIZE = 20;

typedef struct Pixel {
	int x, y;
	Pixel *nextPixel;
} Pixel;

typedef struct blob {
	Pixel *pixels;
	int size;
	blob *nextBlob;
} blob;

int runNetwork(Net net, Mat img, UCHAR *output);
int filter_rgb(UINT8 r, UINT8 g, UINT8 b);
void upload_mask();
void show_mask(const rect& r);
blob *createBlob(int x, int y);
Pixel *createPixels(int x, int y, int *size);
