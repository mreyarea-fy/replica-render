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

DEFINE_int32(imageSize, 640, "The output image width.");

DEFINE_bool(renderRGBEnable, true, "Render RGB image.");
DEFINE_bool(renderDepthEnable, false, "Render depth maps.");
DEFINE_bool(renderMotionVectorEnable, false, "Render motion flow.");

DEFINE_double(texture_exposure, 1.0, "The texture  exposure.");
DEFINE_double(texture_gamma, 1.0, "The texture gamma.");
DEFINE_double(texture_saturation, 1.0, "The texture saturation.");

int main(int argc, char* argv[]) {
  auto model_start = std::chrono::high_resolution_clock::now();

  // 0) parser the input arguments.
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  FLAGS_stderrthreshold = google::GLOG_INFO;

  LOG(INFO) << "Replica CubeMap rendering.";

  const std::string data_root(FLAGS_data_root);
  fs::directory_entry data_root_dir{ fs::path(data_root) };
  ASSERT(data_root_dir.exists());
  const std::string meshFile(data_root + FLAGS_meshFile);
  ASSERT(pangolin::FileExists(meshFile));
  const std::string atlasFolder(data_root + FLAGS_atlasFolder);
  ASSERT(pangolin::FileExists(atlasFolder));
  std::string surfaceFile = std::string(data_root + FLAGS_mirrorFile);
  // ASSERT(pangolin::FileExists(surfaceFile));
  if(!pangolin::FileExists(surfaceFile)){
      surfaceFile.clear();
      LOG(WARNING) << "Do not set the mirror file.";
  }

  const std::string outputDir = std::string(FLAGS_outputDir);
  fs::directory_entry outputDir_dir{ fs::path(outputDir) };
  ASSERT(outputDir_dir.exists());
  const std::string cameraposeFile(FLAGS_cameraPoseFile);

  const int width = FLAGS_imageSize;
  const int height = FLAGS_imageSize;
  if (width <= 0 || height <= 0 || width != height)
    LOG(ERROR) << "The image size setting error, width is " << width << ", height is " << height << ".";
  bool renderDepth = FLAGS_renderDepthEnable;
  if (renderDepth) LOG(INFO) << "Render depth maps.";
  bool renderMotionFlow = FLAGS_renderMotionVectorEnable;
  if (renderMotionFlow) LOG(INFO) << "Render Motion Vector.";
  bool renderRGB = FLAGS_renderRGBEnable;
  if (renderRGB) LOG(INFO) << "Render RGB images.";

  float depthScale = 1.0f;//65535.0f * 0.1f;

  // 1) Setup OpenGL Display
#ifdef _WIN32
  pangolin::CreateWindowAndBind("ReplicaViewer", width, height);
  if (glewInit() != GLEW_OK) {
      pango_print_error("Unable to initialize GLEW.");
  }
  if (!checkGLVersion()) {
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
  const GLenum frontFace = GL_CCW;
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
  else{
      LOG(INFO) << "Can not find the camera pose file, generate camera pose.";
      generateMV(cameraMV);
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

  // load mirrors
  std::vector<MirrorSurface> mirrors;
  if (surfaceFile.length()) {
    std::ifstream file(surfaceFile);
    picojson::value json;
    picojson::parse(json, file);

    for (size_t i = 0; i < json.size(); i++) {
      mirrors.emplace_back(json[i]);
    }
    std::cout << "Loaded " << mirrors.size() << " mirrors" << std::endl;
  }

  // load mesh and textures
  PTexMesh ptexMesh(meshFile, atlasFolder);
  ptexMesh.SetExposure(FLAGS_texture_exposure);
  ptexMesh.SetGamma(FLAGS_texture_gamma);
  ptexMesh.SetSaturation(FLAGS_texture_saturation);
  const std::string shadir = STR(SHADER_DIR);
  MirrorRenderer mirrorRenderer(mirrors, width, height, shadir);

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
    Eigen::Matrix4d s_cam_current_mv = s_cam_current.GetModelViewMatrix();
    Eigen::Matrix4d s_cam_next_mv = s_cam_next.GetModelViewMatrix();

    for (int face_index = 0; face_index < 6; ++face_index)
    {
        Eigen::Transform<double, 3, Eigen::Affine> t;
        const char *  face_abbr;
        if (face_index == 0)
        {
            // look +x axis
            t = (Eigen::AngleAxis<double>(0.5 * M_PI, Eigen::Vector3d::UnitY()));
            face_abbr = "R";
        }
        else if (face_index == 1)
        {
            // look -x axis
            t = (Eigen::AngleAxis<double>(-0.5 * M_PI, Eigen::Vector3d::UnitY()));
            face_abbr = "L";
        }
        else if (face_index == 2)
        {
            // look +y axis
            t = (Eigen::AngleAxis<double>(0.5 * M_PI, Eigen::Vector3d::UnitX()));
            face_abbr = "D";
        }
        else if (face_index == 3)
        {
            // look -y axis
            t = (Eigen::AngleAxis<double>(-0.5 * M_PI, Eigen::Vector3d::UnitX()));
            face_abbr = "U";
        }
        else if (face_index == 4)
        {
            //look +z axis
            t = (Eigen::AngleAxis<double>(0, Eigen::Vector3d::UnitY()));
            face_abbr = "F";
        }
        else if (face_index == 5)
        {
            //look -z axis
            t = (Eigen::AngleAxis<double>(M_PI, Eigen::Vector3d::UnitY()));
            face_abbr = "B";
        }
        Eigen::Matrix4d camera_direction = t.matrix().inverse();
        s_cam_current.GetModelViewMatrix() = Eigen::Matrix4d(camera_direction * s_cam_current_mv);
        s_cam_next.GetModelViewMatrix() = Eigen::Matrix4d(camera_direction * s_cam_next_mv);

        // Render
        if (renderRGB)
        {
            LOG(INFO) << "Render CubeMap RGB images " << frame_index << " face " << face_abbr;
            frameBuffer.Bind();
            glPushAttrib(GL_VIEWPORT_BIT);
            glViewport(0, 0, width, height);
            glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
            glEnable(GL_CULL_FACE);
            ptexMesh.Render(s_cam_current);
            glDisable(GL_CULL_FACE);
            glPopAttrib(); //GL_VIEWPORT_BIT
            frameBuffer.Unbind();

            for (size_t face_index = 0; face_index < mirrors.size(); face_index++)
            {
                MirrorSurface& mirror = mirrors[face_index];
                // capture reflections
                mirrorRenderer.CaptureReflection(mirror, ptexMesh, s_cam_current, frontFace);
                frameBuffer.Bind();
                glPushAttrib(GL_VIEWPORT_BIT);
                glViewport(0, 0, width, height);
                // render mirror
                mirrorRenderer.Render(mirror, mirrorRenderer.GetMaskTexture(face_index), s_cam_current);
                glPopAttrib(); //GL_VIEWPORT_BIT
                frameBuffer.Unbind();
            }

            // Download and save
            render.Download(image.ptr, GL_RGB, GL_UNSIGNED_BYTE);
            char cubemapFilename[1024];
            snprintf(cubemapFilename, 1024, "%s/%04zu_%s_rgb.jpg", outputDir.c_str(), frame_index, face_abbr);
            pangolin::SaveImage(image.UnsafeReinterpret<uint8_t>(),
                pangolin::PixelFormatFromString("RGB24"),
                std::string(cubemapFilename));
        }

        if (renderDepth) 
        {
            LOG(INFO) << "Render CubeMap depth maps " << frame_index << " face " << face_abbr;
            depthFrameBuffer.Bind();
            glPushAttrib(GL_VIEWPORT_BIT);
            glViewport(0, 0, width, height);
            glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
            glClearNamedFramebufferfv(depthFrameBuffer.fbid, GL_COLOR, 0, depthClearValue);
            glEnable(GL_CULL_FACE);
            ptexMesh.RenderDepth(s_cam_current, 1.0);
            glDisable(GL_CULL_FACE);
            glPopAttrib(); //GL_VIEWPORT_BIT
            depthFrameBuffer.Unbind();
            depthTexture.Download(depthImage.ptr, GL_RED, GL_FLOAT);
            char depthfilename[1024];
            snprintf(depthfilename, 1024, "%s/%04zu_%s_depth.dpt", outputDir.c_str(), frame_index, face_abbr);
            saveDepthmap2dpt(depthfilename, depthImage.ptr, width, height);
        }

        if (renderMotionFlow)
        {
            LOG(INFO) << "Render CubeMap depth forward optical flow " << frame_index << " face " << face_abbr;
            // 0) render optical flow (current frame to next frame)
            opticalflowFrameBuffer.Bind();
            glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
            glPushAttrib(GL_VIEWPORT_BIT);
            glViewport(0, 0, width, height);
            glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
            // glFrontFace(GL_CCW);      //Don't draw backfaces
            glEnable(GL_CULL_FACE);
            // glDisable(GL_CULL_FACE);
            // glDisable(GL_LINE_SMOOTH);
            // glDisable(GL_POLYGON_SMOOTH);
            // glDisable(GL_MULTISAMPLE);
            ptexMesh.RenderMotionVector(s_cam_current, s_cam_next, width, height);
            // glEnable(GL_MULTISAMPLE);
            glPopAttrib(); //GL_VIEWPORT_BIT
            opticalflowFrameBuffer.Unbind();
            opticalflowTexture.Download(opticalFlow_forward.ptr, GL_RGBA, GL_FLOAT);
            char filename[1024];
            snprintf(filename, 1024, "%s/%04zu_%s_motionvector_forward.flo", outputDir.c_str(), frame_index, face_abbr);
            saveMotionVector(filename, opticalFlow_forward.ptr, width, height, true); // output optical flow to file
            // save the target points depth.

            // 1) render optical flow (next frame to current frame)
            LOG(INFO) << "Render CubeMap depth backward optical flow " << frame_index << " face " << face_abbr;
            opticalflowFrameBuffer.Bind();
            glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
            glPushAttrib(GL_VIEWPORT_BIT);
            glViewport(0, 0, width, height);
            glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
            // glFrontFace(GL_CCW);      //Don't draw backfaces
            glEnable(GL_CULL_FACE);
            // glDisable(GL_CULL_FACE);
            // glDisable(GL_LINE_SMOOTH);
            // glDisable(GL_POLYGON_SMOOTH);
            // glDisable(GL_MULTISAMPLE);
            ptexMesh.RenderMotionVector(s_cam_next, s_cam_current, width, height);
            // glEnable(GL_MULTISAMPLE);
            glPopAttrib(); //GL_VIEWPORT_BIT
            opticalflowFrameBuffer.Unbind();
            opticalflowTexture.Download(opticalFlow_backward.ptr, GL_RGBA, GL_FLOAT);
            snprintf(filename, 1024, "%s/%04zu_%s_motionvector_backward.flo", outputDir.c_str(), (frame_index + 1) % numFrames, face_abbr);
            saveMotionVector(filename, opticalFlow_backward.ptr, width, height, true); // output optical flow to file
        }
    }
  }
  auto model_stop = std::chrono::high_resolution_clock::now();
  auto model_duration = std::chrono::duration_cast<std::chrono::microseconds>(model_stop - model_start);

  std::cout << "Time taken rendering the model: " << model_duration.count() << " microseconds" << std::endl;

  return 0;
}

