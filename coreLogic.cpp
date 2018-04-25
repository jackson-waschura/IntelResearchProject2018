#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API
#include <librealsense2/hpp/rs_internal.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <wchar.h>
#include <iostream>
#include <cmath>
#include "example.hpp"
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/core/utils/trace.hpp>
#include <fstream>
#include <iostream>
#include <cstdlib>

#define _ATL_APARTMENT_THREADED

#include <atlbase.h>
extern CComModule _Module;
#include <atlcom.h>
//#include <stdafx.h>
#include <sapi.h>

using namespace std;
using namespace cv;
using namespace cv::dnn;

GLvoid *mask_pixels;
GLuint gl_handle;

int filter_rgb(UINT8 r, UINT8 g, UINT8 b);
blob *createBlob(int x, int y);
Pixel *createPixels(int x, int y, int *size);
void upload_mask();
void show_mask(const rect& r);
int frame_count = 0;
float temporal_record[3][TEMP_BUFFER_SIZE];

static int NUM_CLASSES = 21;
static int S_SIZE = H;
static String MODEL_TXT = "fcn8s-heavy-pascal.prototxt";
static String MODEL_BIN = "fcn8s-heavy-pascal.caffemodel";

int runNetwork(Net net, Mat img, UCHAR *output) {
	//FCN takes 500 x 500 images
	Mat inputBlob = blobFromImage(img, 1.0f, Size(S_SIZE, S_SIZE),
		Scalar(104, 117, 123), false);   //Convert Mat to batch of images
	net.setInput(inputBlob, "data");        //set the network input
	Mat prob;         //compute output
	cv::TickMeter t;
	CV_TRACE_REGION("forward");
	net.setInput(inputBlob, "data");
	prob = net.forward("score");

	int width = prob.size[2];
	int height = prob.size[3];
	Mat result(S_SIZE, S_SIZE, CV_8UC3, Vec3b(0, 0, 0));

	for (int i = 0; i < width; i++) {
		for (int j = 0; j < height; j++) {
			int first[4] = { 0, 0, j, i };
			float max_val = prob.at<float>(first);
			unsigned char max_idx = 0;

			for (unsigned char x = 0; x < NUM_CLASSES; x++) {
				int indx[4] = { 0, x, j, i };
				float next = prob.at<float>(indx);

				if (max_val <= next) {
					max_val = next;
					max_idx = x;
				}
			}
			
			Vec3b pixel;
			pixel[0] = (max_idx * 13) % 255;
			pixel[1] = (max_idx * 19) % 255;
			pixel[2] = (max_idx * 29) % 255;
			result.at<Vec3b>(Point(i, j)) = pixel;
			/*
			output[i + (j * width)] = (max_idx * 13) % 255;
			output[i + (j * width) + 1] = (max_idx * 19) % 255;
			output[i + (j * width) + 2] = (max_idx * 29) % 255;
			*/
		}
	}
	memcpy(output, result.data, H * H * 3);
	
	return 0;
}

int main(int argc, char * argv[]) try
{
	Mat networkInput(H, H, CV_8UC3, Vec3b(0, 0, 0));
	UINT8 *colorFrame = NULL;
	UINT16 *depthFrame = NULL;
	int tempIndex = 0;

	window app(W * 2, H, "RealSense Capture Example");

	texture color_image;

	for (tempIndex = 0; tempIndex < TEMP_BUFFER_SIZE; tempIndex++) {
		temporal_record[0][tempIndex] = 0.0;
		temporal_record[1][tempIndex] = 0.0;
		temporal_record[2][tempIndex] = 0.0;
	}

	CV_TRACE_FUNCTION();

	Net net;
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

	while (app)
	{
		printf("getting frame:\n");
		frames = pipe.wait_for_frames();

		auto aligned_frames = align.proccess(frames);
		rs2::video_frame color = aligned_frames.first(RS2_STREAM_COLOR);
		rs2::depth_frame depth = aligned_frames.get_depth_frame();

		colorFrame = (UINT8*)color.get_data();

		// Build pointcloud
		points = pc.calculate(depth);
		auto vertices = points.get_vertices();

		// Create mask by filtering RGB values
		int offset = (W - H) / 2;
		for (int x = offset; x < W-offset; x++) {
			for (int y = 0; y < H; y++) {
				UINT8 R = colorFrame[3 * (x + y * W)];
				UINT8 G = colorFrame[3 * (x + y * W) + 1];
				UINT8 B = colorFrame[3 * (x + y * W) + 2];
				
				Vec3b pixel;
				pixel[0] = R;
				pixel[1] = G;
				pixel[2] = B;
				networkInput.at<Vec3b>(Point(x-offset, y)) = pixel;
				/*
				if (filter_rgb(R, G, B)) {
					((UINT8 *)mask_pixels)[3 * (x + y * W)] = TARGET_RED;
					((UINT8 *)mask_pixels)[3 * (x + y * W) + 1] = TARGET_GREEN;
					((UINT8 *)mask_pixels)[3 * (x + y * W) + 2] = TARGET_BLUE;
				}
				else {
					((UINT8 *)mask_pixels)[3 * (x + y * W)] = 0x00;
					((UINT8 *)mask_pixels)[3 * (x + y * W) + 1] = 0x00;
					((UINT8 *)mask_pixels)[3 * (x + y * W) + 2] = 0x00;
				}
				*/
			}
		}
		UCHAR networkOutput[H * H * 3];
		runNetwork(net, networkInput, networkOutput);
		fprintf(stderr, "Made it !\n");
		mask_pixels = networkOutput;
		// Separate into blobs, and determine the largest blob
		blob *blobsHead = NULL, *blobsTail = NULL, *blobsTemp = NULL, *largestBlob = NULL;
		for (int x = 0; x < H; x++) {
			for (int y = 0; y < H; y++) {
				if (networkOutput[3 * (x + y * H)]) {
					blobsTemp = createBlob(x, y);
					if (!blobsTemp) {
						printf("ERROR! Out of Memory!");
						return EXIT_FAILURE;
					}
					if (!blobsHead) {
						blobsHead = blobsTemp;
					}
					else {
						blobsTail->nextBlob = blobsTemp;
					}
					blobsTail = blobsTemp;

					if (!largestBlob || largestBlob->size < blobsTemp->size) {
						largestBlob = blobsTemp;
					}
				}
			}
		}

		fprintf(stderr, "Largest Blob !\n");

		if(largestBlob) printf("largest blob size: %d\n", largestBlob->size);

		float totalX = 0.0, totalY = 0.0, totalZ = 0.0;
		int count = 0;
		// Only fil back the largest Blob (and average it's vertices from the pointcloud)
		Pixel *pixelsPtr = NULL;

		if (largestBlob) {
			pixelsPtr = largestBlob->pixels;
			count = largestBlob->size;
			while (pixelsPtr) {
				((UINT8 *)mask_pixels)[3 * (pixelsPtr->x + pixelsPtr->y * H)] = TARGET_RED;
				((UINT8 *)mask_pixels)[3 * (pixelsPtr->x + pixelsPtr->y * H) + 1] = TARGET_GREEN;
				((UINT8 *)mask_pixels)[3 * (pixelsPtr->x + pixelsPtr->y * H) + 2] = TARGET_BLUE;

				totalX += vertices[pixelsPtr->x + offset + pixelsPtr->y * H].x;
				totalY += vertices[pixelsPtr->x + offset + pixelsPtr->y * H].y;
				totalZ += vertices[pixelsPtr->x + offset + pixelsPtr->y * H].z;
				pixelsPtr = pixelsPtr->nextPixel;
			}
		}

		fprintf(stderr, "Summmed Pixels!\n");

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

		temporal_record[0][frame_count % TEMP_BUFFER_SIZE] = avgX;
		temporal_record[1][frame_count % TEMP_BUFFER_SIZE] = avgY;
		temporal_record[2][frame_count % TEMP_BUFFER_SIZE] = avgZ;

		if (frame_count >= TEMP_BUFFER_SIZE) {
			avgX = 0.0;
			avgY = 0.0;
			avgZ = 0.0;
			for (tempIndex = 0; tempIndex < TEMP_BUFFER_SIZE; tempIndex++) {
				avgX += temporal_record[0][tempIndex];
				avgX += temporal_record[1][tempIndex];
				avgX += temporal_record[2][tempIndex];
			}
			avgX /= TEMP_BUFFER_SIZE;
			avgY /= TEMP_BUFFER_SIZE;
			avgZ /= TEMP_BUFFER_SIZE;
		}

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
			WCHAR *object_name = L"Cat"; // hardcoded for now
			WCHAR *direction = avgX > 0 ? L"right" : L"left";
			WCHAR message[100];
			swprintf(message, sizeof(message), L"%s is %.1f meters away, %.1f meters to your %s, and %.1f meters above the ground",
				object_name, avgZ, avgX, direction, avgY);
			//hr = pVoice->Speak(L"Hello world", 0, NULL);
			const WCHAR *realMessage = (const WCHAR *)message;
			hr = pVoice->Speak(realMessage, 0, NULL);
			pVoice->Release();
			pVoice = NULL;
		}

		::CoUninitialize();

		Mat image(H, H, CV_8UC3, Vec3b(0, 0, 0));

		memcpy(image.data, mask_pixels, H * H * 3);
		imshow("Mask", image);
		waitKey(1);
		/*
		color_image.render(color, { 0, 0, app.width() / 2, app.height() });
		upload_mask();
		rect r = { app.width() / 2, 0, app.width() / 2, app.height() };
		show_mask(r.adjust_ratio({ float(W), float(H) }));
		*/
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

int filter_rgb(UINT8 r, UINT8 g, UINT8 b) {
	return (r - TARGET_RED) * (r - TARGET_RED)
		+ (g - TARGET_GREEN) * (g - TARGET_GREEN)
		+ (b - TARGET_BLUE) * (b - TARGET_BLUE)
		< TARGET_DIST * TARGET_DIST;
}

void upload_mask()
{
	if (!gl_handle)
		glGenTextures(1, &gl_handle);
	GLenum err = glGetError();

	glBindTexture(GL_TEXTURE_2D, gl_handle);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, H, H, 0, GL_RGB, GL_UNSIGNED_BYTE, mask_pixels);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void show_mask(const rect& r)
{
	if (!gl_handle) return;

	glBindTexture(GL_TEXTURE_2D, gl_handle);
	glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUAD_STRIP);
	glTexCoord2f(0.f, 1.f); glVertex2f(r.x, r.y + r.h);
	glTexCoord2f(0.f, 0.f); glVertex2f(r.x, r.y);
	glTexCoord2f(1.f, 1.f); glVertex2f(r.x + r.w, r.y + r.h);
	glTexCoord2f(1.f, 0.f); glVertex2f(r.x + r.w, r.y);
	glEnd();
	glDisable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);
}

// Returns a new pointer to a blob or NULL if error
blob *createBlob(int x, int y) {
	blob *returnBlob = (blob *)malloc(sizeof(blob));
	if (!returnBlob) {
		return NULL;
	}
	returnBlob->nextBlob = NULL;
	returnBlob->size = 0;
	returnBlob->pixels = createPixels(x, y, &(returnBlob->size));
	//printf("blob at (%d, %d) with size %d\n", x, y, returnBlob->size);
	return returnBlob;
}

// Returns a new pointer to a pixel or NULL is error
Pixel *createPixels(int x, int y, int *size) {
	UINT8 *pixels = (UINT8 *)mask_pixels;
	Pixel *headPixel = NULL,
		*queueHead = NULL,
		*queueTail = NULL;

	queueHead = (Pixel *)malloc(sizeof(Pixel));
	if (!queueHead)
		return NULL;
	queueHead->x = x;
	queueHead->y = y;
	queueHead->nextPixel = NULL;
	queueTail = queueHead;
	headPixel = queueHead;
	((UINT8 *)mask_pixels)[3 * (x + y * H)] = 0;
	((UINT8 *)mask_pixels)[3 * (x + y * H) + 1] = 0;
	((UINT8 *)mask_pixels)[3 * (x + y * H) + 2] = 0;
	(*size)++;

	// while there are still undiscovered pixels
	while (queueHead) {
		// add pixel to the left of this pixel
		if (queueHead->x - 1 >= 0 && ((UINT8 *)mask_pixels)[3 * ((queueHead->x - 1) + queueHead->y * H)]) {
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
			((UINT8 *)mask_pixels)[3 * (queueTail->x + queueTail->y * H)] = 0;
			((UINT8 *)mask_pixels)[3 * (queueTail->x + queueTail->y * H) + 1] = 0;
			((UINT8 *)mask_pixels)[3 * (queueTail->x + queueTail->y * H) + 2] = 0;
			(*size)++;
		}
		// add pixel below this pixel
		if (queueHead->y + 1 < H && ((UINT8 *)mask_pixels)[3 * (queueHead->x + (queueHead->y + 1) * H)]) {
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
			((UINT8 *)mask_pixels)[3 * (queueTail->x + queueTail->y * H)] = 0;
			((UINT8 *)mask_pixels)[3 * (queueTail->x + queueTail->y * H) + 1] = 0;
			((UINT8 *)mask_pixels)[3 * (queueTail->x + queueTail->y * H) + 2] = 0;
			(*size)++;
		}
		// add pixel above this pixel
		if (queueHead->y - 1 >= 0 && ((UINT8 *)mask_pixels)[3 * (queueHead->x + (queueHead->y - 1) * H)]) {
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
			((UINT8 *)mask_pixels)[3 * (queueTail->x + queueTail->y * H)] = 0;
			((UINT8 *)mask_pixels)[3 * (queueTail->x + queueTail->y * H) + 1] = 0;
			((UINT8 *)mask_pixels)[3 * (queueTail->x + queueTail->y * H) + 2] = 0;
			(*size)++;
		}
		// add pixel to the right of this pixel
		if (queueHead->x + 1 < H && ((UINT8 *)mask_pixels)[3 * ((queueHead->x + 1) + queueHead->y * H)]) {
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
			((UINT8 *)mask_pixels)[3 * (queueTail->x + queueTail->y * H)] = 0;
			((UINT8 *)mask_pixels)[3 * (queueTail->x + queueTail->y * H) + 1] = 0;
			((UINT8 *)mask_pixels)[3 * (queueTail->x + queueTail->y * H) + 2] = 0;
			(*size)++;
		}
		queueHead = queueHead->nextPixel;
	}

	return headPixel;
}
