#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API
#include <librealsense2/hpp/rs_internal.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <wchar.h>
#include <iostream>
#include <cmath>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <atlbase.h>
#include <atlcom.h>
#include <sapi.h>
#include "coreLogic.h"
#include "Network.h"

using namespace std;
using namespace cv;
using namespace cv::dnn;

blob *createBlob(int x, int y, UCHAR *mask);
Pixel *createPixels(int x, int y, int *size, UCHAR *mask);

static String MODEL_TXT = "fcn8s-heavy-pascal.prototxt";
static String MODEL_BIN = "fcn8s-heavy-pascal.caffemodel";

static int RUNNING = 1;

int main(int argc, char * argv[]) try
{
	Mat networkInput(H, H, CV_8UC3, Vec3b(0, 0, 0));
	UCHAR *colorFrame = NULL;
	UINT16 *depthFrame = NULL;

	CV_TRACE_FUNCTION();

	Net net;

	// TODO: Put this in a function in the Network.cpp file (loadNetwork)
	try {
		net = dnn::readNetFromCaffe(MODEL_TXT, MODEL_BIN);
	}
	catch (cv::Exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
		if (net.empty()) {
			std::cerr << "Can't load network by using the following files: " << std::endl;
			std::cerr << "prototxt:   " << MODEL_TXT << std::endl;
			std::cerr << "caffemodel: " << MODEL_BIN << std::endl;
			exit(-1);
		}
	}
	net.setPreferableTarget(DNN_TARGET_CPU);

	// Declare pointcloud object, for calculating pointclouds and texture mappings
	rs2::pointcloud pc;
	// We want the points object to be persistent so we can display the last cloud when a frame drops
	rs2::points points;
	// Declare RealSense pipeline, encapsulating the actual device and sensors
	rs2::pipeline pipe;
	// declare Frameset object
	rs2::frameset frames;
	// Start streaming with default recommended configuration
	rs2::config cfg;
	// Use a configuration object to request only depth from the pipeline
	cfg.enable_stream(RS2_STREAM_DEPTH, W, H, RS2_FORMAT_Z16, 30);
	cfg.enable_stream(RS2_STREAM_COLOR, W, H, RS2_FORMAT_RGB8, 30);
	rs2::align align(RS2_STREAM_COLOR);
	pipe.start(cfg);

	while (RUNNING)
	{
		frames = pipe.wait_for_frames();

		// align depth and color frames
		auto aligned_frames = align.proccess(frames);
		rs2::video_frame color = aligned_frames.first(RS2_STREAM_COLOR);
		rs2::depth_frame depth = aligned_frames.get_depth_frame();

		colorFrame = (UCHAR*)color.get_data();

		// Build pointcloud
		points = pc.calculate(depth);
		auto vertices = points.get_vertices();

		// Create mask by filtering RGB values
		int offset = (W - H) / 2;
		for (int x = offset; x < W - offset; x++) {
			for (int y = 0; y < H; y++) {
				UCHAR R = colorFrame[3 * (x + y * W)];
				UCHAR G = colorFrame[3 * (x + y * W) + 1];
				UCHAR B = colorFrame[3 * (x + y * W) + 2];

				Vec3b pixel;
				pixel[0] = R;
				pixel[1] = G;
				pixel[2] = B;
				networkInput.at<Vec3b>(Point(x - offset, y)) = pixel;
			}
		}

		UCHAR networkOutput[H * H];
		runNetwork(net, networkInput, networkOutput);

		// TODO: Put this into its own file "localization.cpp"??
		// Separate into blobs, and determine the largest blob
		blob *blobsHead = NULL, *blobsTail = NULL, *blobsTemp = NULL, *largestBlob = NULL;
		for (int x = 0; x < H; x++) {
			for (int y = 0; y < H; y++) {
				if (networkOutput[x + y * H]) {
					blobsTemp = createBlob(x, y, networkOutput);
					if (!blobsTemp) {
						fprintf(stderr, "ERROR! Out of Memory!");
						return EXIT_FAILURE;
					}
					if (!blobsHead) {
						blobsHead = blobsTemp;
					}
					else {
						blobsTail->nextBlob = blobsTemp;
					}
					blobsTail = blobsTemp;

					// found new largest blob
					if (!largestBlob || largestBlob->size < blobsTemp->size) {
						largestBlob = blobsTemp;
					}
				}
			}
		}

		if (largestBlob) printf("largest blob size: %d\n", largestBlob->size);

		float totalX = 0.0, totalY = 0.0, totalZ = 0.0;
		int count = 0;
		// Only fill back the largest Blob (and average it's vertices from the pointcloud)
		Pixel *pixelsPtr = NULL;

		// find the average x, y, and z coordinates for the largest blob
		if (largestBlob) {
			pixelsPtr = largestBlob->pixels;
			count = largestBlob->size;
			while (pixelsPtr) {
				((UCHAR *)networkOutput)[pixelsPtr->x + pixelsPtr->y * H] = TARGET_ID;

				totalX += vertices[pixelsPtr->x + offset + pixelsPtr->y * H].x;
				totalY += vertices[pixelsPtr->x + offset + pixelsPtr->y * H].y;
				totalZ += vertices[pixelsPtr->x + offset + pixelsPtr->y * H].z;
				pixelsPtr = pixelsPtr->nextPixel;
			}
		}

		// Free all of the blobs and pixel lists
		Pixel *pixelFree = NULL;
		while (blobsHead) {
			pixelsPtr = blobsHead->pixels;
			while (pixelsPtr) {
				pixelFree = pixelsPtr;
				pixelsPtr = pixelsPtr->nextPixel;
				free(pixelFree);
			}
			blobsTemp = blobsHead;
			blobsHead = blobsHead->nextBlob;
			free(blobsTemp);
		}

		float avgX = count == 0 ? 0 : totalX / count;
		float avgY = count == 0 ? 0 : totalY / count;
		float avgZ = count == 0 ? 0 : totalZ / count;

		printf("Average Of (%d) Stuff: %f, %f, %f\n", count, avgX, avgY, avgZ);

		ISpVoice * pVoice = NULL;

		// initialize COM for SAPI
		if (FAILED(::CoInitialize(NULL)))
			return FALSE;

		// create COM voice object
		HRESULT hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void **)&pVoice);
		if (SUCCEEDED(hr))
		{
			// speak
			WCHAR *object_name = L"person"; // hardcoded for now
			WCHAR *direction = avgX > 0 ? L"right" : L"left";
			WCHAR message[100];
			swprintf(message, sizeof(message), L"The %s is %.1f meters away, %.1f meters to your %s, and %.1f meters above the ground",
				object_name, avgZ, avgX, direction, avgY);
			//hr = pVoice->Speak(L"Hello world", 0, NULL);
			const WCHAR *realMessage = (const WCHAR *)message;
			hr = pVoice->Speak(realMessage, 0, NULL);
			pVoice->Release();
			pVoice = NULL;
		}

		::CoUninitialize();

		Mat image(H, H, CV_8U);

		for (int i = 0; i < H * H; i++) {
			image.data[i] = (networkOutput[i] == 0 ? 0 : 255);
		}

		namedWindow("Mask", WINDOW_AUTOSIZE);
		imshow("Mask", image);
		waitKey(4000);
		destroyWindow("Mask");
	}
	return EXIT_SUCCESS;
}
catch (const rs2::error & e)
{
	std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
	return EXIT_FAILURE;
}
catch (const std::exception & e)
{
	std::cerr << e.what() << std::endl;
	return EXIT_FAILURE;
}

// Returns a new pointer to a blob or NULL if error
blob *createBlob(int x, int y, UCHAR *mask) {
	blob *returnBlob = (blob *)malloc(sizeof(blob));
	if (!returnBlob) {
		return NULL;
	}
	returnBlob->nextBlob = NULL;
	returnBlob->size = 0;
	returnBlob->pixels = createPixels(x, y, &(returnBlob->size), mask);
	return returnBlob;
}

// Returns a new pointer to a pixel or NULL is error
Pixel *createPixels(int x, int y, int *size, UCHAR *mask) {
	Pixel *headPixel = NULL,
		*queueHead = NULL,
		*queueTail = NULL;

	// create the start of the pixels linked list
	queueHead = (Pixel *)malloc(sizeof(Pixel));
	if (!queueHead)
		return NULL;
	queueHead->x = x;
	queueHead->y = y;
	queueHead->nextPixel = NULL;
	queueTail = queueHead;
	headPixel = queueHead;
	((UCHAR *)mask)[x + y * H] = 0;
	(*size)++;

	// while there are still undiscovered pixels
	while (queueHead) {
		// add pixel to the left of this pixel
		if (queueHead->x - 1 >= 0 && ((UCHAR *)mask)[(queueHead->x - 1) + queueHead->y * H]) {
			// create new pixel
			queueTail->nextPixel = (Pixel *)malloc(sizeof(Pixel));
			if (!queueTail->nextPixel) {
				return NULL;
			}
			// enqueue pixel
			queueTail = queueTail->nextPixel;
			queueTail->x = queueHead->x - 1;
			queueTail->y = queueHead->y;
			queueTail->nextPixel = NULL;
			((UCHAR *)mask)[queueTail->x + queueTail->y * H] = 0;
			(*size)++;
		}
		// add pixel below this pixel
		if (queueHead->y + 1 < H && ((UCHAR *)mask)[queueHead->x + (queueHead->y + 1) * H]) {
			// create new pixel
			queueTail->nextPixel = (Pixel *)malloc(sizeof(Pixel));
			if (!queueTail->nextPixel) {
				return NULL;
			}
			// enqueue pixel
			queueTail = queueTail->nextPixel;
			queueTail->x = queueHead->x;
			queueTail->y = queueHead->y + 1;
			queueTail->nextPixel = NULL;
			((UCHAR *)mask)[queueTail->x + queueTail->y * H] = 0;
			(*size)++;
		}
		// add pixel above this pixel
		if (queueHead->y - 1 >= 0 && ((UCHAR *)mask)[queueHead->x + (queueHead->y - 1) * H]) {
			// create new pixel
			queueTail->nextPixel = (Pixel *)malloc(sizeof(Pixel));
			if (!queueTail->nextPixel) {
				return NULL;
			}
			// enqueue pixel
			queueTail = queueTail->nextPixel;
			queueTail->x = queueHead->x;
			queueTail->y = queueHead->y - 1;
			queueTail->nextPixel = NULL;
			((UCHAR *)mask)[queueTail->x + queueTail->y * H] = 0;
			(*size)++;
		}
		// add pixel to the right of this pixel
		if (queueHead->x + 1 < H && ((UCHAR *)mask)[(queueHead->x + 1) + queueHead->y * H]) {
			// create new pixel
			queueTail->nextPixel = (Pixel *)malloc(sizeof(Pixel));
			if (!queueTail->nextPixel) {
				return NULL;
			}
			// enqueue pixel
			queueTail = queueTail->nextPixel;
			queueTail->x = queueHead->x + 1;
			queueTail->y = queueHead->y;
			queueTail->nextPixel = NULL;
			((UCHAR *)mask)[queueTail->x + queueTail->y * H] = 0;
			(*size)++;
		}
		queueHead = queueHead->nextPixel;
	}

	return headPixel;
}