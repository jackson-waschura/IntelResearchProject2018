#pragma once
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/core/utils/trace.hpp>

using namespace cv;
using namespace cv::dnn;

// needed for sapi
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

// holds a pixel and a pointer to the next pixel in an image
typedef struct Pixel {
	int x, y;
	Pixel *nextPixel;
} Pixel;

/* used to store information about pixels in each blob, size of the blob,
	and a pointer to another blob where a blob is a groupt of pixels adjacent to each other */
typedef struct blob {
	Pixel *pixels;
	int size;
	blob *nextBlob;
} blob;


int runNetwork(Net net, Mat img, UCHAR *output);
int filter_rgb(UINT8 r, UINT8 g, UINT8 b);
void upload_mask();
void show_mask(const rect& r);
// createBlob takes in the coordinates of a pixel and returns the blob of containing that pixel
blob *createBlob(int x, int y);
/* createPixels takes in the coordinates of a pixel and the size of a blob
	and returns a pointer to the head of a linked list containing all of the pixels in that blob 
	returns NULL on error */
Pixel *createPixels(int x, int y, int *size);
