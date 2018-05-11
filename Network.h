#pragma once

#include <opencv2/dnn.hpp>

int runNetwork(cv::dnn::experimental_dnn_v4::Net net, cv::Mat img, UCHAR *output);