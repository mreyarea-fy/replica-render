#include <PTexLib.h>
#include <pangolin/image/image_convert.h>
#include <GLCheck.h>
#include <MirrorRenderer.h>
#include <DataIO.h>
#include <EGL.h>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

DEFINE_string(data_root, "", "The root folder of Replica scene data.");
DEFINE_string(meshFile, "", "The mesh file path.");
DEFINE_string(atlasFolder, "", "The atlas folder path.");
DEFINE_string(mirrorFile, "", "The mirror file path.");
DEFINE_string(cameraPoseFile, "", "The camera pose file path.");
DEFINE_string(outputDir, "", "The data output folder path.");

DEFINE_int32(imageHeight, 640, "The output image width.");

DEFINE_bool(renderRGBEnable, true, "Render RGB image.");
DEFINE_bool(renderDepthEnable, false, "Render depth maps.");
DEFINE_bool(renderMotionVectorEnable, false, "Render motion flow.");

DEFINE_double(texture_exposure, 1.0, "The texture  exposure.");
DEFINE_double(texture_gamma, 1.0, "The texture gamma.");
DEFINE_double(texture_saturation, 1.0, "The texture saturation.");

int main(int argc, char *argv[])
{
  auto model_start = std::chrono::high_resolution_clock::now();

  // 0) parser the input arguments.
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  FLAGS_stderrthreshold = google::GLOG_INFO;

  LOG(INFO) << "Replica Panoramic rendering.";

  const std::string data_root(FLAGS_data_root);
  fs::directory_entry data_root_dir{fs::path(data_root)};
  ASSERT(data_root_dir.exists());
  const std::string meshFile(data_root + FLAGS_meshFile);
  ASSERT(pangolin::FileExists(meshFile));
  const std::string atlasFolder(data_root + FLAGS_atlasFolder);
  ASSERT(pangolin::FileExists(atlasFolder));
  const std::string surfaceFile = std::string(data_root + FLAGS_mirrorFile);
  if (surfaceFile.length() > 0)
    LOG(WARNING) << "The Panoramic render do not support mirror rendering.";
  //ASSERT(pangolin::FileExists(surfaceFile));

  const std::string outputDir = std::string(FLAGS_outputDir);
  fs::directory_entry outputDir_dir{fs::path(outputDir)};
  ASSERT(outputDir_dir.exists());
  const std::string cameraposeFile(FLAGS_cameraPoseFile);

  const int width = FLAGS_imageHeight * 2;
  const int height = FLAGS_imageHeight;
  if (width <= 0 || height <= 0 || width != 2 * height)
    LOG(ERROR) << "The image size setting error, width is " << width << ", height is " << height << ".";
  bool renderDepth = FLAGS_renderDepthEnable;
  if (renderDepth)
    LOG(INFO) << "Render depth maps.";
  bool renderMotionFlow = FLAGS_renderMotionVectorEnable;
  if (renderMotionFlow)
    LOG(INFO) << "Render Motion Vector.";
  bool renderRGB = FLAGS_renderRGBEnable;
  if (renderRGB)
    LOG(INFO) << "Render RGB images.";

  float depthScale = 1.0f; //65535.0f * 0.1f;

  // 1) Setup OpenGL Display
#ifdef _WIN32
  pangolin::CreateWindowAndBind("ReplicaViewer", width, height);
  if (glewInit() != GLEW_OK)
  {
    pango_print_error("Unable to initialize GLEW.");
  }
  if (!checkGLVersion())
  {
    return 1;
  }
#elif __linux__
    // Setup EGL
  EGLCtx egl;
  egl.PrintInformation();
  
  if(!checkGLVersion()) {
    return 1;
  }
#endif

  // Don't draw backfaces
  glEnable(GL_DEPTH_TEST);
  GLfloat depthClearValue[] = { -10.0 };
  const GLenum frontFace = GL_CW;
  glFrontFace(frontFace);

  // Setup a framebuffer
  pangolin::GlRenderBuffer renderBuffer(width, height);
  pangolin::GlTexture render(width, height);
  pangolin::GlFramebuffer frameBuffer(render, renderBuffer);
  pangolin::GlTexture depthTexture(width, height, GL_R32F, true, 0, GL_RED, GL_FLOAT);
  pangolin::GlFramebuffer depthFrameBuffer(depthTexture, renderBuffer); // to render depth image
  pangolin::GlTexture opticalflowTexture(width, height, GL_RGBA32F);
  pangolin::GlFramebuffer opticalflowFrameBuffer(opticalflowTexture, renderBuffer); // to render motion

  // 2) load camera pose
  std::vector<pangolin::OpenGlMatrix> cameraMV;
  if (pangolin::FileExists(cameraposeFile))
    loadMV(cameraposeFile, cameraMV);
  else
  {
    LOG(INFO) << "Can not find the camera pose file, generate camera pose.";
    generateMV(cameraMV, 3);
  }
  // Setup a camera get MVP
  pangolin::OpenGlRenderState s_cam_current(
      pangolin::ProjectionMatrixRDF_BottomLeft(
          width,
          height,
          width / 2.0f,
          width / 2.0f,
          (width - 1.0f) / 2.0f,
          (height - 1.0f) / 2.0f,
          0.1f,
          100.0f),
      pangolin::ModelViewLookAtRDF(1, 0, 0, 0, 0, -1, 0, 1, 0));
  pangolin::OpenGlRenderState s_cam_next;
  s_cam_next.GetProjectionMatrix() = s_cam_current.GetProjectionMatrix();

  //// TODO load mirrors, rendering 360 image's mirror
  //std::vector<MirrorSurface> mirrors;
  //if (surfaceFile.length())
  //{
  //  std::ifstream file(surfaceFile);
  //  picojson::value json;
  //  picojson::parse(json, file);

  //  for (size_t i = 0; i < json.size(); i++)
  //  {
  //    mirrors.emplace_back(json[i]);
  //  }
  //  std::cout << "Loaded " << mirrors.size() << " mirrors" << std::endl;
  //}

  // load mesh and textures
  PTexMesh ptexMesh(meshFile, atlasFolder);
  ptexMesh.SetExposure(FLAGS_texture_exposure);
  ptexMesh.SetGamma(FLAGS_texture_gamma);
  ptexMesh.SetSaturation(FLAGS_texture_saturation);
  const std::string shadir = STR(SHADER_DIR);
  //MirrorRenderer mirrorRenderer(mirrors, width, height, shadir);

  // Render some frames
  pangolin::ManagedImage<Eigen::Matrix<uint8_t, 3, 1>> image(width, height);
  pangolin::ManagedImage<Eigen::Matrix<float, 1, 1>> depthImage(width, height);
  pangolin::ManagedImage<Eigen::Matrix<float, 4, 1>> opticalFlow_forward(width, height);
  pangolin::ManagedImage<Eigen::Matrix<float, 4, 1>> opticalFlow_backward(width, height);

  const size_t numFrames = cameraMV.size();
  for (size_t frame_index = 0; frame_index < numFrames; frame_index++)
  {
    LOG(INFO) << "\rRendering frame " << frame_index + 1 << "/" << numFrames << "... ";

    // 0) load & update the camera pose & MV matrix
    s_cam_current.SetModelViewMatrix(cameraMV[frame_index]);
    s_cam_next.SetModelViewMatrix(cameraMV[(frame_index + 1) % numFrames]);

    // Render
    if (renderRGB)
    {
      LOG(INFO) << "Render Panoramic RGB images " << frame_index;
      frameBuffer.Bind();
      glPushAttrib(GL_VIEWPORT_BIT);
      glViewport(0, 0, width, height);
      glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
      //ptexMesh.SetExposure(0.01);
      glDisable(GL_CULL_FACE);
      glEnable(GL_DEPTH_TEST);
      ptexMesh.RenderPano(s_cam_current);
      glEnable(GL_CULL_FACE);
      glPopAttrib(); //GL_VIEWPORT_BIT
      frameBuffer.Unbind();

      //for (size_t face_index = 0; face_index < mirrors.size(); face_index++)
      //{
      //  MirrorSurface &mirror = mirrors[face_index];
      //  // capture reflections
      //  mirrorRenderer.CaptureReflection(mirror, ptexMesh, s_cam_current, frontFace);
      //  frameBuffer.Bind();
      //  glPushAttrib(GL_VIEWPORT_BIT);
      //  glViewport(0, 0, width, height);
      //  // render mirror
      //  mirrorRenderer.Render(mirror, mirrorRenderer.GetMaskTexture(face_index), s_cam_current);
      //  glPopAttrib(); //GL_VIEWPORT_BIT
      //  frameBuffer.Unbind();
      //}

      // Download and save
      render.Download(image.ptr, GL_RGB, GL_UNSIGNED_BYTE);
      char cubemapFilename[1024];
      snprintf(cubemapFilename, 1024, "%s/%04zu_rgb_pano.jpg", outputDir.c_str(), frame_index);
      pangolin::SaveImage(image.UnsafeReinterpret<uint8_t>(),
                          pangolin::PixelFormatFromString("RGB24"),
                          std::string(cubemapFilename));
    }

    if (renderDepth)
    {
        LOG(INFO) << "Render Panoramic depth maps " << frame_index;
        depthFrameBuffer.Bind();
        glPushAttrib(GL_VIEWPORT_BIT);
        glViewport(0, 0, width, height);
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
        glClearNamedFramebufferfv(depthFrameBuffer.fbid, GL_COLOR, 0, depthClearValue);
        glEnable(GL_CULL_FACE);
        ptexMesh.RenderPanoDepth(s_cam_current, depthScale);
        glDisable(GL_CULL_FACE);
        glPopAttrib(); //GL_VIEWPORT_BIT
        depthFrameBuffer.Unbind();
        depthTexture.Download(depthImage.ptr, GL_RED, GL_FLOAT);
        char depthfilename[1024];
        snprintf(depthfilename, 1024, "%s/%04zu_depth_pano.dpt", outputDir.c_str(), frame_index);
        saveDepthmap2dpt(depthfilename, depthImage.ptr, width, height);
    }

     if (renderMotionFlow)
     {
       LOG(INFO) << "Render CubeMap depth forward optical flow " << frame_index;
       // 0) render optical flow (current frame to next frame)
       opticalflowFrameBuffer.Bind();
       glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
       glPushAttrib(GL_VIEWPORT_BIT);
       glViewport(0, 0, width, height);
       glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
       // glFrontFace(GL_CCW);      //Don't draw backfaces
       // glEnable(GL_CULL_FACE);
       glDisable(GL_CULL_FACE);
       glDisable(GL_LINE_SMOOTH);
       glDisable(GL_POLYGON_SMOOTH);
       glDisable(GL_MULTISAMPLE);
       ptexMesh.RenderPanoMotionVector(s_cam_current, s_cam_next, width, height);
       glDisable(GL_CULL_FACE);
       glEnable(GL_MULTISAMPLE);
       glPopAttrib(); //GL_VIEWPORT_BIT
       opticalflowFrameBuffer.Unbind();
       opticalflowTexture.Download(opticalFlow_forward.ptr, GL_RGBA, GL_FLOAT);
       char filename[1024];
       snprintf(filename, 1024, "%s/%04zu_motionvector_forward.flo", outputDir.c_str(), frame_index);
       saveMotionVector(filename, opticalFlow_forward.ptr, width, height); // output optical flow to file

       // 1) render optical flow (next frame to current frame)
       LOG(INFO) << "Render CubeMap depth backward optical flow " << frame_index;
       opticalflowFrameBuffer.Bind();
       glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
       glPushAttrib(GL_VIEWPORT_BIT);
       glViewport(0, 0, width, height);
       glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
       // glFrontFace(GL_CCW);      //Don't draw backfaces
       // glEnable(GL_CULL_FACE);
       glDisable(GL_CULL_FACE);
       glDisable(GL_LINE_SMOOTH);
       glDisable(GL_POLYGON_SMOOTH);
       glDisable(GL_MULTISAMPLE);
       ptexMesh.RenderPanoMotionVector(s_cam_next, s_cam_current, width, height);
       glDisable(GL_CULL_FACE);
       glEnable(GL_MULTISAMPLE);
       glPopAttrib(); //GL_VIEWPORT_BIT
       opticalflowFrameBuffer.Unbind();
       opticalflowTexture.Download(opticalFlow_backward.ptr, GL_RGBA, GL_FLOAT);
       snprintf(filename, 1024, "%s/%04zu_motionvector_backward.flo", outputDir.c_str(), (frame_index + 1) % numFrames);
       saveMotionVector(filename, opticalFlow_backward.ptr, width, height); // output optical flow to file
     }
  }
  auto model_stop = std::chrono::high_resolution_clock::now();
  auto model_duration = std::chrono::duration_cast<std::chrono::microseconds>(model_stop - model_start);
  std::cout << "Time taken rendering the model: " << model_duration.count() << " microseconds" << std::endl;
  return 0;
}
