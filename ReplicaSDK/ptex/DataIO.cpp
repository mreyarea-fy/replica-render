
#include <EGL.h>
#include <PTexLib.h>
#include <string>
#include <pangolin/image/image_convert.h>
#include <Eigen/Geometry>
#include <chrono>
#include <random>
#include <iterator>
#include <iostream>
#include <fstream>
//#include <DepthMeshLib.h>
#include <DataIO.h>

void saveMotionVector(const char *filename, const void *ptr, const int width, const int height, const bool target_depth_enable)
{
  std::ofstream flo_file(filename, std::ios::binary);
  if (!flo_file)
  {
    return;
  }
  else
  {
    // save the optical flow to *.flo
    flo_file << "PIEH";
    flo_file.write((char *)&width, sizeof(int));
    flo_file.write((char *)&height, sizeof(int));
    for (int i = 0; i < width * height; i++)
    {
      flo_file.write(static_cast<const char *>((char *)ptr + sizeof(float) * 4 * i), 2 * sizeof(float)); // output the first to channel
    }
    if (target_depth_enable)
    {
      // save the target points depth to *.dpt
      std::string dptFilePath = std::string(filename) + ".dpt";
      float* targetDepth = (float*)malloc(width * height * sizeof(float));
      for (int i = 0; i < width * height; i++)
      {
          targetDepth[i] = ((float*)(ptr))[4 * i + 2];
      }
      saveDepthmap2dpt(dptFilePath.c_str(), (void *)targetDepth, width, height);
      free(targetDepth);
    }
  }
  flo_file.close();
}

// write the depth map to a .dpt file (Sintel format).
void saveDepthmap2dpt(const char *filename, const void *ptr, const int width, const int height)
{
  if (filename == nullptr)
  {
    std::cout << "Error in " << __FUNCTION__ << ": empty filename.";
    return;
  }

  const char *dot = strrchr(filename, '.');
  if (dot == nullptr)
  {
    std::cout << "Error in " << __FUNCTION__ << ": extension required in filename " << filename << ".";
    return;
  }

  if (strcmp(dot, ".dpt") != 0)
  {
    std::cout << "Error in " << __FUNCTION__ << ": filename " << filename << " should have extension '.dpt'.";
    return;
  }

  FILE *stream = fopen(filename, "wb");
  if (stream == 0)
  {
    std::cout << "Error in " << __FUNCTION__ << ": could not open " << filename;
    return;
  }

  // write the header
  fprintf(stream, "PIEH");
  if ((int)fwrite(&width, sizeof(int), 1, stream) != 1 || (int)fwrite(&height, sizeof(int), 1, stream) != 1)
  {
    std::cout << "Error in writeSintelDptFile(" << filename << "): problem writing header.";
    return;
  }

  // write the depth data
  fwrite(ptr, 1, width * height * sizeof(float), stream);

  fclose(stream);
}

void loadMV(const std::string &navPositions,
             std::vector<pangolin::OpenGlMatrix> &cameraMV)
{
  std::fstream in(navPositions);
  std::string line;
  int i = 0;
  std::vector<std::vector<float>> cameraPose;
  while (std::getline(in, line))
  {
    float value;
    std::stringstream ss(line);
    cameraPose.push_back(std::vector<float>());

    while (ss >> value)
    {
      cameraPose[i].push_back(value);
    }

    // transform the camera position and rotation parameter to MV matrix
    Eigen::Matrix3f model_mat;
    model_mat = Eigen::AngleAxisf(cameraPose[i][6] / 180.0 * M_PI, Eigen::Vector3f::UnitZ()) * 
                Eigen::AngleAxisf(cameraPose[i][5] / 180.0 * M_PI, Eigen::Vector3f::UnitY()) * 
                Eigen::AngleAxisf(cameraPose[i][4] / 180.0 * M_PI, Eigen::Vector3f::UnitX());

    Eigen::Vector3f eye_point(cameraPose[i][1], cameraPose[i][2], cameraPose[i][3]);
    Eigen::Vector3f target_point = eye_point + model_mat * Eigen::Vector3f(1, 0, 0);
    Eigen::Vector3f up = Eigen::Vector3f::UnitZ();

    // +x right, -y up, +z forward
    pangolin::OpenGlMatrix mv = pangolin::ModelViewLookAtRDF(
        eye_point[0], eye_point[1], eye_point[2],
        target_point[0], target_point[1], target_point[2],
        up[0], up[1], up[2]);

    cameraMV.push_back(mv);

    ++i;
  }

  std::stringstream ss;
  ss << "there are :" << i << " cameras pose in the file :" << navPositions << std::endl;
  std::cout << ss.str() << std::endl;
}


void generateMV(std::vector<pangolin::OpenGlMatrix> &cameraMV, const unsigned int step_number)
{
  Eigen::Matrix4d T_new_old ;
  T_new_old << 1, 0, 0, 0,
               0, 0, -1, 0,
               0, 1, 0, 0,
               0, 0, 0, 1;
  Eigen::Matrix4d transformation_mat;
  transformation_mat <<   1, 0, 0, 0,
                 0, 1, 0, 0,
                 0, 0, 1, 0,
                 0, 0, 0, 1;
  transformation_mat.topRightCorner(3, 1) = Eigen::Vector3d(0.25, 0, 0);
  for (int i = 0; i < step_number; i++)
  {
    cameraMV.push_back(pangolin::OpenGlMatrix(T_new_old));
    T_new_old = T_new_old * transformation_mat.inverse();
  }
}

// void load_mv(const std::string &navPositions,
//              std::vector<std::vector<float>> &cameraPose,
//              std::vector<pangolin::OpenGlMatrix> &cameraMV)
// {
//   std::fstream in(navPositions);
//   std::string line;
//   int i = 0;
//   while (std::getline(in, line))
//   {
//     float value;
//     std::stringstream ss(line);
//     cameraPose.push_back(std::vector<float>());

//     while (ss >> value)
//     {
//       cameraPose[i].push_back(value);
//     }

//     // transform the camera position and rotation parameter to MV matrix
//     Eigen::Matrix3f model_mat;
//     model_mat = Eigen::AngleAxisf(cameraPose[i][6] / 180.0 * M_PI, Eigen::Vector3f::UnitZ()) *
//                 Eigen::AngleAxisf(cameraPose[i][5] / 180.0 * M_PI, Eigen::Vector3f::UnitY()) *
//                 Eigen::AngleAxisf(cameraPose[i][4] / 180.0 * M_PI, Eigen::Vector3f::UnitX());

//     Eigen::Vector3f eye_point(cameraPose[i][1], cameraPose[i][2], cameraPose[i][3]);
//     Eigen::Vector3f target_point = eye_point + model_mat * Eigen::Vector3f(1, 0, 0);
//     Eigen::Vector3f up = Eigen::Vector3f::UnitZ();

//     // +x right, -y up, +z forward
//     pangolin::OpenGlMatrix mv = pangolin::ModelViewLookAtRDF(
//         eye_point[0], eye_point[1], eye_point[2],
//         target_point[0], target_point[1], target_point[2],
//         up[0], up[1], up[2]);

//     cameraMV.push_back(mv);

//     ++i;
//   }

//   std::stringstream ss;
//   ss << "there are :" << i << " cameras pose in the file :" << navPositions << std::endl;
//   std::cout << ss.str() << std::endl;
// }