#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include "coreLogic.h"
#include "Network.h"

using namespace cv;
using namespace cv::dnn;

int runNetwork(Net net, Mat img, UCHAR *output) {
	//FCN takes 500 x 500 images
	cv::Mat inputBlob = blobFromImage(img, 1.0f, Size(S_SIZE, S_SIZE),
		Scalar(104, 117, 123), false);   //Convert Mat to batch of images
	net.setInput(inputBlob, "data");        //set the network input
	cv::Mat prob;         //compute output
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
		}
	}
	memcpy(output, result.data, H * H * 3);

	return 0;
}