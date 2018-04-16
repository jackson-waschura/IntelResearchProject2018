#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API
#include <librealsense2/hpp/rs_internal.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <iostream>
#include <cmath>
#include "example.hpp"

const int W = 640;
const int H = 480;
const int TARGET_RED = 0xC5;
const int TARGET_GREEN = 0x1B;
const int TARGET_BLUE = 0x77;
const int TARGET_DIST = 90;
const int TEMP_BUFFER_SIZE = 20;

GLvoid *mask_pixels = malloc(sizeof(UINT8) * W * H * 3);
GLuint gl_handle;

typedef struct Pixel {
	int x, y;
	Pixel *nextPixel;
} Pixel;

typedef struct blob {
	Pixel *pixels;
	int size;
	blob *nextBlob;
} blob;

int filter_rgb(UINT8 r, UINT8 g, UINT8 b);
blob *createBlob(int x, int y);
Pixel *createPixels(int x, int y, int *size);
void upload_mask();
void show_mask(const rect& r);
int frame_count = 0;
float temporal_record[3][TEMP_BUFFER_SIZE];

int main(int argc, char * argv[]) try
{
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
		for (int x = 0; x < W; x++) {
			for (int y = 0; y < H; y++) {
				UINT8 R = colorFrame[3 * (x + y * W)];
				UINT8 G = colorFrame[3 * (x + y * W) + 1];
				UINT8 B = colorFrame[3 * (x + y * W) + 2];
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
			}
		}

		// Separate into blobs, and determine the largest blob
		blob *blobsHead = NULL, *blobsTail = NULL, *blobsTemp = NULL, *largestBlob = NULL;
		for (int x = 0; x < W; x++) {
			for (int y = 0; y < H; y++) {
				if (((UINT8 *)mask_pixels)[3 * (x + y * W)]) {
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

		if(largestBlob) printf("largest blob size: %d\n", largestBlob->size);

		float totalX = 0.0, totalY = 0.0, totalZ = 0.0;
		int count = 0;
		// Only fil back the largest Blob (and average it's vertices from the pointcloud)
		Pixel *pixelsPtr = NULL;

		if (largestBlob) {
			pixelsPtr = largestBlob->pixels;
			count = largestBlob->size;
			while (pixelsPtr) {
				((UINT8 *)mask_pixels)[3 * (pixelsPtr->x + pixelsPtr->y * W)] = TARGET_RED;
				((UINT8 *)mask_pixels)[3 * (pixelsPtr->x + pixelsPtr->y * W) + 1] = TARGET_GREEN;
				((UINT8 *)mask_pixels)[3 * (pixelsPtr->x + pixelsPtr->y * W) + 2] = TARGET_BLUE;

				totalX += vertices[pixelsPtr->x + pixelsPtr->y * W].x;
				totalY += vertices[pixelsPtr->x + pixelsPtr->y * W].y;
				totalZ += vertices[pixelsPtr->x + pixelsPtr->y * W].z;
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

		color_image.render(color, { 0, 0, app.width() / 2, app.height() });
		upload_mask();
		rect r = { app.width() / 2, 0, app.width() / 2, app.height() };
		show_mask(r.adjust_ratio({ float(W), float(H) }));
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

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, W, H, 0, GL_RGB, GL_UNSIGNED_BYTE, mask_pixels);

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
	((UINT8 *)mask_pixels)[3 * (x + y * W)] = 0;
	((UINT8 *)mask_pixels)[3 * (x + y * W) + 1] = 0;
	((UINT8 *)mask_pixels)[3 * (x + y * W) + 2] = 0;
	(*size)++;

	// while there are still undiscovered pixels
	while (queueHead) {
		// add pixel to the left of this pixel
		if (queueHead->x - 1 >= 0 && ((UINT8 *)mask_pixels)[3 * ((queueHead->x - 1) + queueHead->y * W)]) {
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
			((UINT8 *)mask_pixels)[3 * (queueTail->x + queueTail->y * W)] = 0;
			((UINT8 *)mask_pixels)[3 * (queueTail->x + queueTail->y * W) + 1] = 0;
			((UINT8 *)mask_pixels)[3 * (queueTail->x + queueTail->y * W) + 2] = 0;
			(*size)++;
		}
		// add pixel below this pixel
		if (queueHead->y + 1 < H && ((UINT8 *)mask_pixels)[3 * (queueHead->x + (queueHead->y + 1) * W)]) {
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
			((UINT8 *)mask_pixels)[3 * (queueTail->x + queueTail->y * W)] = 0;
			((UINT8 *)mask_pixels)[3 * (queueTail->x + queueTail->y * W) + 1] = 0;
			((UINT8 *)mask_pixels)[3 * (queueTail->x + queueTail->y * W) + 2] = 0;
			(*size)++;
		}
		// add pixel above this pixel
		if (queueHead->y - 1 >= 0 && ((UINT8 *)mask_pixels)[3 * (queueHead->x + (queueHead->y - 1) * W)]) {
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
			((UINT8 *)mask_pixels)[3 * (queueTail->x + queueTail->y * W)] = 0;
			((UINT8 *)mask_pixels)[3 * (queueTail->x + queueTail->y * W) + 1] = 0;
			((UINT8 *)mask_pixels)[3 * (queueTail->x + queueTail->y * W) + 2] = 0;
			(*size)++;
		}
		// add pixel to the right of this pixel
		if (queueHead->x + 1 < W && ((UINT8 *)mask_pixels)[3 * ((queueHead->x + 1) + queueHead->y * W)]) {
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
			((UINT8 *)mask_pixels)[3 * (queueTail->x + queueTail->y * W)] = 0;
			((UINT8 *)mask_pixels)[3 * (queueTail->x + queueTail->y * W) + 1] = 0;
			((UINT8 *)mask_pixels)[3 * (queueTail->x + queueTail->y * W) + 2] = 0;
			(*size)++;
		}
		queueHead = queueHead->nextPixel;
	}

	return headPixel;
}
