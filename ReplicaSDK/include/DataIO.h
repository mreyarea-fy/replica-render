// Copyright (c) Facebook, Inc. and its affiliates. All Rights Reserved
#pragma once

#include <string>
#include <vector>

// save optical flow to *.flo files
void saveMotionVector(const char *filename, const void *ptr, const int width, const int height, const bool targetDepthEnable = false);

// write the depth map to a .dpt file (Sintel format).
void saveDepthmap2dpt(const char *filename, const void *ptr, const int width, const int height);

/**
 * @brief Load camera pose from *.csv file
 * 
 * @param navPositions The *.csv file absolute path.
 * @param cameraMV  The camera orientation.
 */
void loadMV(const std::string &navPositions, std::vector<pangolin::OpenGlMatrix> &cameraMV);

/**
 * @brief Generate the camera pose moving along the X axis.
 */
void generateMV(std::vector<pangolin::OpenGlMatrix> &cameraMV, const unsigned int step_number = 4);