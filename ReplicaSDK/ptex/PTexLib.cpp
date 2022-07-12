// Copyright (c) Facebook, Inc. and its affiliates. All Rights Reserved
#include "PTexLib.h"
#include "PLYParser.h"

#include <pangolin/utils/file_utils.h>
#include <pangolin/utils/picojson.h>
#include <Eigen/Geometry>

 #include <filesystem>

#ifdef _WIN32
  #include <iostream>
  #include <sort_linux.hpp>
#elif __linux__
  #include <fcntl.h>
  #include <sys/mman.h>
  #include <unistd.h>
#endif

#include "FileMemMap.h"

#include <fstream>
#include <unordered_map>

PTexMesh::PTexMesh(const std::string& meshFile, const std::string& atlasFolder, const bool panoramic_enable) {
  // Check everything exists
  ASSERT(pangolin::FileExists(meshFile));
  ASSERT(pangolin::FileExists(atlasFolder));

  // Parse parameters
  const std::string paramsFile = atlasFolder + "/parameters.json";

  ASSERT(pangolin::FileExists(paramsFile));

  std::ifstream file(paramsFile);
  picojson::value json;
  picojson::parse(json, file);

  ASSERT(json.contains("splitSize"), "Missing splitSize in parameters.json");
  ASSERT(json.contains("tileSize"), "Missing tileSize in parameters.json");

  splitSize = json["splitSize"].get<double>();
  tileSize = json["tileSize"].get<int64_t>();

  LoadMeshData(meshFile);

  LoadAtlasData(atlasFolder);
  if (isHdr) {
    // set defaults for HDR scene
    exposure = 0.025f;
    gamma = 1.6969f;
    saturation = 1.5f;
  }

  // Load shader
  const std::string shadir = STR(SHADER_DIR);
  ASSERT(pangolin::FileExists(shadir), "Shader directory not found!");

  shader.AddShaderFromFile(pangolin::GlSlVertexShader, shadir + "/mesh-ptex.vert", {}, {shadir});
  shader.AddShaderFromFile(pangolin::GlSlGeometryShader, shadir + "/mesh-ptex.geom", {}, {shadir});
  shader.AddShaderFromFile(pangolin::GlSlFragmentShader, shadir + "/mesh-ptex.frag", {}, {shadir});
  shader.Link();

  shaderPano.AddShaderFromFile(pangolin::GlSlVertexShader, shadir + "/mesh-ptex-pano.vert", {}, {shadir});
  shaderPano.AddShaderFromFile(pangolin::GlSlGeometryShader, shadir + "/mesh-ptex-pano.geom", {}, {shadir});
  shaderPano.AddShaderFromFile(pangolin::GlSlFragmentShader, shadir + "/mesh-ptex-pano.frag", {}, {shadir});
  shaderPano.Link();

  depthShader.AddShaderFromFile(pangolin::GlSlVertexShader, shadir + "/mesh-depth.vert", {}, {shadir});
  depthShader.AddShaderFromFile(pangolin::GlSlFragmentShader, shadir + "/mesh-depth.frag", {}, {shadir});
  depthShader.Link();

  depthPanoShader.AddShaderFromFile(pangolin::GlSlVertexShader, shadir + "/mesh-ptex-pano-depth.vert", {}, { shadir });
  depthPanoShader.AddShaderFromFile(pangolin::GlSlGeometryShader, shadir + "/mesh-ptex-pano-depth.geom", {}, { shadir });
  depthPanoShader.AddShaderFromFile(pangolin::GlSlFragmentShader, shadir + "/mesh-ptex-pano-depth.frag", {}, { shadir });
  depthPanoShader.Link();

  motionVectorShader.AddShaderFromFile(pangolin::GlSlVertexShader, shadir + "/mesh-motionflow.vert", {}, {shadir});
  motionVectorShader.AddShaderFromFile(pangolin::GlSlGeometryShader, shadir + "/mesh-motionflow.geom", {}, {shadir});
  motionVectorShader.AddShaderFromFile(pangolin::GlSlFragmentShader, shadir + "/mesh-motionflow.frag", {}, {shadir});
  motionVectorShader.Link();

  motionVectorPanoShader.AddShaderFromFile(pangolin::GlSlVertexShader, shadir + "/mesh-ptex-pano-motionflow.vert", {}, {shadir});
  motionVectorPanoShader.AddShaderFromFile(pangolin::GlSlGeometryShader, shadir + "/mesh-ptex-pano-motionflow.geom", {}, {shadir});
  motionVectorPanoShader.AddShaderFromFile(pangolin::GlSlFragmentShader, shadir + "/mesh-ptex-pano-motionflow.frag", {}, {shadir});
  motionVectorPanoShader.Link();
}

PTexMesh::~PTexMesh() {}

float PTexMesh::Exposure() const {
  return exposure;
}

void PTexMesh::SetExposure(const float& val) {
  exposure = val;
}

float PTexMesh::Gamma() const {
  return gamma;
}

void PTexMesh::SetGamma(const float& val) {
  gamma = val;
}

float PTexMesh::Saturation() const {
  return saturation;
}

void PTexMesh::SetSaturation(const float& val) {
  saturation = val;
}

void PTexMesh::RenderSubMesh(
    size_t subMesh,
    const pangolin::OpenGlRenderState& cam,
    const Eigen::Vector4f& clipPlane) {
  ASSERT(subMesh < meshes.size());
  Mesh& mesh = *meshes[subMesh];

  shader.Bind();
  shader.SetUniform("MVP", cam.GetProjectionModelViewMatrix());
  shader.SetUniform("tileSize", (int)tileSize);
  shader.SetUniform("exposure", exposure);
  shader.SetUniform("gamma", 1.0f / gamma);
  shader.SetUniform("saturation", saturation);
  shader.SetUniform("clipPlane", clipPlane(0), clipPlane(1), clipPlane(2), clipPlane(3));

  shader.SetUniform("widthInTiles", int(mesh.atlas.width / tileSize));

  glActiveTexture(GL_TEXTURE0);
  mesh.atlas.Bind();

  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mesh.abo.bo);

  mesh.vbo.Bind();
  glVertexAttribPointer(0, mesh.vbo.count_per_element, mesh.vbo.datatype, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(0);
  mesh.vbo.Unbind();

  mesh.ibo.Bind();
  // using GL_LINES_ADJACENCY here to send quads to geometry shader
  glDrawElements(GL_LINES_ADJACENCY, mesh.ibo.num_elements, mesh.ibo.datatype, 0);
  mesh.ibo.Unbind();

  glDisableVertexAttribArray(0);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);

  glActiveTexture(GL_TEXTURE0);
  mesh.atlas.Unbind();

  shader.Unbind();
}

void PTexMesh::RenderPanoSubMesh(
    size_t subMesh,
    const pangolin::OpenGlRenderState &cam)
{
  ASSERT(subMesh < meshes.size());
  Mesh& mesh = *meshes[subMesh];

  shaderPano.Bind();
  shaderPano.SetUniform("MV", cam.GetModelViewMatrix());
  shaderPano.SetUniform("tileSize", (int)tileSize);
  shaderPano.SetUniform("exposure", exposure);
  shaderPano.SetUniform("gamma", 1.0f / gamma);
  shaderPano.SetUniform("saturation", saturation);
  shaderPano.SetUniform("widthInTiles", int(mesh.atlas.width / tileSize));

  glActiveTexture(GL_TEXTURE0);
  mesh.atlas.Bind();

  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mesh.abo.bo);

  mesh.vbo.Bind();
  glVertexAttribPointer(0, mesh.vbo.count_per_element, mesh.vbo.datatype, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(0);
  mesh.vbo.Unbind();

  mesh.ibo.Bind();
  // using GL_LINES_ADJACENCY here to send quads to geometry shader
  glDrawElements(GL_LINES_ADJACENCY, mesh.ibo.num_elements, mesh.ibo.datatype, 0);
  mesh.ibo.Unbind();

  glDisableVertexAttribArray(0);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);

  glActiveTexture(GL_TEXTURE0);
  mesh.atlas.Unbind();

  shaderPano.Unbind();
}

// render depth
void PTexMesh::RenderSubMeshDepth(
    size_t subMesh,
    const pangolin::OpenGlRenderState& cam,
    const float depthScale,
    const Eigen::Vector4f& clipPlane) {
  ASSERT(subMesh < meshes.size());
  Mesh& mesh = *meshes[subMesh];

  glPushAttrib(GL_POLYGON_BIT);
  int currFrontFace;
  glGetIntegerv(GL_FRONT_FACE, &currFrontFace);
  //Drawing the faces has the opposite winding order to the GL_LINES_ADJACENCY
  glFrontFace(currFrontFace == GL_CW ? GL_CCW : GL_CW);

  depthShader.Bind();
  depthShader.SetUniform("MVP", cam.GetProjectionModelViewMatrix());  
  depthShader.SetUniform("MV", cam.GetModelViewMatrix());
  depthShader.SetUniform("clipPlane", clipPlane(0), clipPlane(1), clipPlane(2), clipPlane(3));
  depthShader.SetUniform("scale", depthScale);

  mesh.vbo.Bind();
  glVertexAttribPointer(0, mesh.vbo.count_per_element, mesh.vbo.datatype, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(0);
  mesh.vbo.Unbind();

  mesh.ibo.Bind();
  glDrawElements(GL_QUADS, mesh.ibo.num_elements, mesh.ibo.datatype, 0);
  mesh.ibo.Unbind();
  glDisableVertexAttribArray(0);

  depthShader.Unbind();

  glPopAttrib();
}

void PTexMesh::RenderPanoSubMeshDepth(
    size_t subMesh,
    const pangolin::OpenGlRenderState& cam,
    const float depthScale,
    const Eigen::Vector4f& clipPlane)
{
    ASSERT(subMesh < meshes.size());
    Mesh& mesh = *meshes[subMesh];

    depthPanoShader.Bind();
    depthPanoShader.SetUniform("MV", cam.GetModelViewMatrix());
    depthPanoShader.SetUniform("tileSize", (int)tileSize);
    depthPanoShader.SetUniform("exposure", exposure);
    depthPanoShader.SetUniform("gamma", 1.0f / gamma);
    depthPanoShader.SetUniform("saturation", saturation);
    depthPanoShader.SetUniform("widthInTiles", int(mesh.atlas.width / tileSize));

    glActiveTexture(GL_TEXTURE0);
    mesh.atlas.Bind();

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mesh.abo.bo);

    mesh.vbo.Bind();
    glVertexAttribPointer(0, mesh.vbo.count_per_element, mesh.vbo.datatype, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    mesh.vbo.Unbind();

    mesh.ibo.Bind();
    // using GL_LINES_ADJACENCY here to send quads to geometry shader
    glDrawElements(GL_LINES_ADJACENCY, mesh.ibo.num_elements, mesh.ibo.datatype, 0);
    mesh.ibo.Unbind();

    glDisableVertexAttribArray(0);
    
    mesh.atlas.Unbind();
    depthPanoShader.Unbind();
}

void PTexMesh::RenderSubMeshMotionVector(
    size_t subMesh,
    const pangolin::OpenGlRenderState& cam_currnet,
    const pangolin::OpenGlRenderState& cam_next,
    const int image_width,
    const int image_height,
    const Eigen::Vector4f& clipPlane) {

    ASSERT(subMesh < meshes.size());
    Mesh& mesh = *meshes[subMesh];

  // int currFrontFace;
  // glGetIntegerv(GL_FRONT_FACE, &currFrontFace);
  // //Drawing the faces has the opposite winding order to the GL_LINES_ADJACENCY
  // glFrontFace(currFrontFace == GL_CW ? GL_CCW : GL_CW);
  // glPushAttrib(GL_POLYGON_BIT);

    motionVectorShader.Bind();

    motionVectorShader.SetUniform("MVP_current", cam_currnet.GetProjectionModelViewMatrix());
    motionVectorShader.SetUniform("MVP_next", cam_next.GetProjectionModelViewMatrix());
    motionVectorShader.SetUniform("window_size",(float)image_width, (float)image_height);
    motionVectorShader.SetUniform("clipPlane", clipPlane(0), clipPlane(1), clipPlane(2), clipPlane(3));

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mesh.abo.bo);
    mesh.vbo.Bind();
    glVertexAttribPointer(0, mesh.vbo.count_per_element, mesh.vbo.datatype, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    mesh.vbo.Unbind();

    mesh.ibo.Bind();
    // using GL_LINES_ADJACENCY here to send quads to geometry shader
    glDrawElements(GL_LINES_ADJACENCY, mesh.ibo.num_elements, mesh.ibo.datatype, 0);
    mesh.ibo.Unbind();

    glDisableVertexAttribArray(0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    motionVectorShader.Unbind();
}

void PTexMesh::RenderSubMeshPanoMotionVector(
    size_t subMesh,
    const pangolin::OpenGlRenderState& cam_currnet, 
    const pangolin::OpenGlRenderState& cam_next,
    const int image_width,
    const int image_height,
    const Eigen::Vector4f& clipPlane)
    {
      ASSERT(subMesh < meshes.size());
      Mesh& mesh = *meshes[subMesh];

      motionVectorPanoShader.Bind();
      motionVectorPanoShader.SetUniform("MV_current", cam_currnet.GetModelViewMatrix());
      motionVectorPanoShader.SetUniform("MV_next", cam_next.GetModelViewMatrix());
      motionVectorPanoShader.SetUniform("window_size",(float)image_width, (float)image_height);

      glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mesh.abo.bo);
      mesh.vbo.Bind();
      glVertexAttribPointer(0, mesh.vbo.count_per_element, mesh.vbo.datatype, GL_FALSE, 0, 0);
      glEnableVertexAttribArray(0);
      mesh.vbo.Unbind();

      mesh.ibo.Bind();
      // using GL_LINES_ADJACENCY here to send quads to geometry shader
      glDrawElements(GL_LINES_ADJACENCY, mesh.ibo.num_elements, mesh.ibo.datatype, 0);
      mesh.ibo.Unbind();

      glDisableVertexAttribArray(0);
      glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
      motionVectorPanoShader.Unbind();
    }

void PTexMesh::Render(const pangolin::OpenGlRenderState& cam, const Eigen::Vector4f& clipPlane) {
  for (size_t i = 0; i < meshes.size(); i++) {
    RenderSubMesh(i, cam, clipPlane);
  }
}


void PTexMesh::RenderPano(const pangolin::OpenGlRenderState& cam) {
  for (size_t i = 0; i < meshes.size(); i++) {
    RenderPanoSubMesh(i, cam);
  }
}

void PTexMesh::RenderDepth(const pangolin::OpenGlRenderState& cam, const float depthScale, const Eigen::Vector4f& clipPlane) {
  for (size_t i = 0; i < meshes.size(); i++) {
    RenderSubMeshDepth(i, cam, depthScale, clipPlane);
  }
}

void PTexMesh::RenderPanoDepth(const pangolin::OpenGlRenderState& cam, const float depthScale , const Eigen::Vector4f& clipPlane)
{
  for (size_t i = 0; i < meshes.size(); i++) {
    RenderPanoSubMeshDepth(i, cam, depthScale, clipPlane);
  }
}

void PTexMesh::RenderMotionVector(const pangolin::OpenGlRenderState& cam_currnet, 
    const pangolin::OpenGlRenderState& cam_next,
    const int image_width,
    const int image_height,
    const Eigen::Vector4f& clipPlane) {
  for (size_t i = 0; i < meshes.size(); i++) {
    RenderSubMeshMotionVector(i, cam_currnet, cam_next, image_width, image_height, clipPlane);
  }
}

void PTexMesh::RenderPanoMotionVector(const pangolin::OpenGlRenderState& cam_currnet, 
    const pangolin::OpenGlRenderState& cam_next,
    const int image_width,
    const int image_height,
    const Eigen::Vector4f& clipPlane) {
  for (size_t i = 0; i < meshes.size(); i++) {
    RenderSubMeshPanoMotionVector(i, cam_currnet, cam_next, image_width, image_height, clipPlane);
  }
}

void PTexMesh::RenderWireframe(
        const pangolin::OpenGlRenderState& cam,
        const Eigen::Vector4f& clipPlane) {
  const bool doClip =
      !(clipPlane(0) == 0.f && clipPlane(1) == 0.f && clipPlane(2) == 0.f && clipPlane(3) == 0.f);

  GLdouble eqn[4] = {clipPlane(0), clipPlane(1), clipPlane(2), clipPlane(3)};

  if (doClip) {
    glClipPlane(GL_CLIP_PLANE0, eqn);
    glEnable(GL_CLIP_PLANE0);
  }

  cam.Apply();

  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

  glPushAttrib(GL_LINE_BIT);
  glLineWidth(1.5f);

  glColor4f(0.25f, 0.25f, 0.25f, 1.0f);

  glPushAttrib(GL_POLYGON_BIT);
  glFrontFace(GL_CCW);

  for (size_t i = 0; i < meshes.size(); i++) {
    meshes[i]->vbo.Bind();
    glVertexPointer(meshes[i]->vbo.count_per_element, meshes[i]->vbo.datatype, 0, 0);
    glEnableClientState(GL_VERTEX_ARRAY);

    meshes[i]->ibo.Bind();

    glDrawElements(GL_QUADS, meshes[i]->ibo.num_elements, meshes[i]->ibo.datatype, 0);

    meshes[i]->ibo.Unbind();

    glDisableClientState(GL_VERTEX_ARRAY);
    meshes[i]->vbo.Unbind();
  }

  glPopAttrib();

  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

  if (doClip) {
    glDisable(GL_CLIP_PLANE0);
  }

  glPopAttrib();
}

std::vector<MeshData> PTexMesh::SplitMesh(const MeshData& mesh, const float splitSize) {
  std::vector<uint32_t> verts;
  verts.resize(mesh.vbo.size());

  auto Part1By2 = [](uint64_t x) {
    x &= 0x1fffff; // mask off lower 21 bits
    x = (x | (x << 32)) & 0x1f00000000ffff;
    x = (x | (x << 16)) & 0x1f0000ff0000ff;
    x = (x | (x << 8)) & 0x100f00f00f00f00f;
    x = (x | (x << 4)) & 0x10c30c30c30c30c3;
    x = (x | (x << 2)) & 0x1249249249249249;
    return x;
  };

  auto EncodeMorton3 = [&Part1By2](const Eigen::Vector3i& v) {
    return (Part1By2(v(2)) << 2) + (Part1By2(v(1)) << 1) + Part1By2(v(0));
  };

  Eigen::AlignedBox3f boundingBox;

  for (size_t i = 0; i < mesh.vbo.Area(); i++) {
    boundingBox.extend(mesh.vbo[i].head<3>());
  }

// calculate vertex grid position and code
#pragma omp parallel for
  for (int i = 0; i < mesh.vbo.size(); i++) {
    const Eigen::Vector3f p = mesh.vbo[i].head<3>();
    Eigen::Vector3f pi = (p - boundingBox.min()) / splitSize;
    verts[i] = EncodeMorton3(pi.cast<int>());
  }

  // data structure for sorting faces
  struct SortFace {
    uint32_t index[4];
    uint32_t code;
    size_t originalFace;
  };

  // fill per-face data structures (including codes)
  size_t numFaces = mesh.ibo.size() / 4;
  std::vector<SortFace> faces;
  faces.resize(numFaces);

#pragma omp parallel for
  for (int i = 0; i < numFaces; i++) {
    faces[i].originalFace = i;
    faces[i].code = std::numeric_limits<uint32_t>::max();
    for (int j = 0; j < 4; j++) {
      faces[i].index[j] = mesh.ibo[i * 4 + j];

      // face code is minimum of referenced vertices codes
      faces[i].code = std::min(faces[i].code, verts[faces[i].index[j]]);
    }
  }

  // sort faces by code
#ifdef _WIN32
  auto sort_face = [](std::vector<SortFace>& sort_face_list)
  {
      const int index_ele_size = 4;
      unsigned int* index_data_ptr = (unsigned int*)malloc(sizeof(unsigned int) * sort_face_list.size() * index_ele_size);
      unsigned int* code_ptr = (unsigned int*)malloc(sizeof(unsigned int) * sort_face_list.size());
      unsigned long long* originalFace_ptr = (unsigned long long*)malloc(sizeof(unsigned long long) * sort_face_list.size());

      // convert to array
      for (int i = 0; i < sort_face_list.size(); i++)
      {
          for (int j = 0; j < index_ele_size; j++)
              index_data_ptr[i * index_ele_size + j] = sort_face_list[i].index[j];
          code_ptr[i] = sort_face_list[i].code;
          originalFace_ptr[i] = sort_face_list[i].originalFace;
      }
      int face_number = sort_face_list.size();
      sortface_linux(index_data_ptr, face_number, code_ptr, face_number, originalFace_ptr, face_number);
      printf("%s", "linux sort function");

      // update the sort_face_list
      for (int i = 0; i < sort_face_list.size(); i++)
      {
          for (int j = 0; j < index_ele_size; j++)
              sort_face_list[i].index[j] = index_data_ptr[i * index_ele_size + j];
          sort_face_list[i].code = code_ptr[i];
          sort_face_list[i].originalFace = originalFace_ptr[i];
      }
  };

  sort_face(faces);

  // output to file
  //std::ofstream myfile;
  //myfile.open("d:/faces_info_win32.txt");
  //for (SortFace& sf : faces)
  //{
  //    myfile << sf.code << "\t" <<
  //        sf.index[0] << "\t" <<
  //        sf.index[1] << "\t" <<
  //        sf.index[2] << "\t" <<
  //        sf.index[3] << "\t" <<
  //        sf.originalFace << "\n";
  //}
  //myfile.close();
  //printf("face output finished !!!");

  //// load data from files
  //std::ifstream infile("d:/faces_info_cygwin.txt");

  //uint32_t code_;
  //uint32_t index_0, index_1, index_2, index_3;
  //size_t originalFace_;

  //int counter = 0;
  //while (infile >> code_ >> index_0 >> index_1 >> index_2 >> index_3 >> originalFace_)
  //{
  //    // successfully extracted one line, data is in x1, ..., x4, c.
  //    SortFace sf;
  //    faces[counter].code = code_;
  //    faces[counter].index[0] = index_0;
  //    faces[counter].index[1] = index_1;
  //    faces[counter].index[2] = index_2;
  //    faces[counter].index[3] = index_3;
  //    faces[counter].originalFace = originalFace_;
  //    //faces[counter] = (sf);
  //    counter++;
  //    if (counter % 1000 == 0 )
  //        printf("%d\n", counter);
  //}
  //infile.close();
#elif __linux__

  std::sort(faces.begin(), faces.end(), [](const SortFace &f1, const SortFace &f2) -> bool
            { return (f1.code < f2.code); });

#endif

  // find face chunk start indices
  std::vector<uint32_t> chunkStart;
  chunkStart.push_back(0);
  uint32_t prevCode = faces[0].code;
  for (size_t i = 1; i < faces.size(); i++) {
    if (faces[i].code != prevCode) {
      chunkStart.push_back(i);
      prevCode = faces[i].code;
    }
  }

  chunkStart.push_back(faces.size());
  size_t numChunks = chunkStart.size() - 1;

  size_t maxFaces = 0;
  for (size_t i = 0; i < numChunks; i++) {
    uint32_t chunkSize = chunkStart[i + 1] - chunkStart[i];
    if (chunkSize > maxFaces)
      maxFaces = chunkSize;
  }

  // create new mesh for each chunk of faces
  std::vector<MeshData> subMeshes;

  for (size_t i = 0; i < numChunks; i++) {
    subMeshes.emplace_back(4);
  }

#pragma omp parallel for
  for (int i = 0; i < numChunks; i++) {
    uint32_t chunkSize = chunkStart[i + 1] - chunkStart[i];

    std::vector<uint32_t> refdVerts;
    std::unordered_map<uint32_t, uint32_t> refdVertsMap;
    subMeshes[i].ibo.Reinitialise(chunkSize * 4, 1);

    for (size_t j = 0; j < chunkSize; j++) {
      size_t faceIdx = chunkStart[i] + j;
      for (int k = 0; k < 4; k++) {
        uint32_t vertIndex = faces[faceIdx].index[k];
        uint32_t newIndex = 0;

        auto it = refdVertsMap.find(vertIndex);

        if (it == refdVertsMap.end()) {
          // vertex not found, add
          newIndex = refdVerts.size();
          refdVerts.push_back(vertIndex);
          refdVertsMap[vertIndex] = newIndex;
        } else {
          // found, use existing index
          newIndex = it->second;
        }
        subMeshes[i].ibo[j * 4 + k] = newIndex;
      }
    }

    // add referenced vertices to submesh
    subMeshes[i].vbo.Reinitialise(refdVerts.size(), 1);
    subMeshes[i].nbo.Reinitialise(refdVerts.size(), 1);
    for (size_t j = 0; j < refdVerts.size(); j++) {
      uint32_t index = refdVerts[j];
      subMeshes[i].vbo[j] = mesh.vbo[index];
      subMeshes[i].nbo[j] = mesh.nbo[index];
    }
  }

  return subMeshes;
}

void PTexMesh::CalculateAdjacency(const MeshData& mesh, std::vector<uint32_t>& adjFaces) {
  struct EdgeData {
    EdgeData(int f, int e) : face(f), edge(e) {}
    int face;
    int edge;
  };

  std::unordered_map<uint64_t, std::vector<EdgeData>> edgeMap;

  ASSERT(mesh.polygonStride == 4, "Only works on quad meshes");

  size_t numFaces = mesh.ibo.size() / mesh.polygonStride;

  pangolin::ManagedImage<std::unordered_map<uint64_t, std::vector<EdgeData>>::iterator>
      edgeIterators(mesh.polygonStride, numFaces);

  // for each face
  for (size_t f = 0; f < numFaces; f++) {
    // for each edge
    for (int e = 0; e < (int)mesh.polygonStride; e++) {
      // add to edge to face map
      const uint32_t i0 = mesh.ibo[f * mesh.polygonStride + e];
      const uint32_t i1 = mesh.ibo[f * mesh.polygonStride + ((e + 1) % mesh.polygonStride)];
      const uint64_t key = (uint64_t)std::min(i0, i1) << 32 | (uint32_t)std::max(i0, i1);

      const EdgeData edgeData(f, e);

      auto it = edgeMap.find(key);

      if (it == edgeMap.end()) {
        it = edgeMap.emplace(key, std::vector<EdgeData>()).first;
        it->second.reserve(4);
        it->second.push_back(edgeData);
      } else {
        it->second.push_back(edgeData);
      }

      edgeIterators(e, f) = it;
    }
  }

  adjFaces.resize(numFaces * mesh.polygonStride);

  for (size_t f = 0; f < numFaces; f++) {
    for (int e = 0; e < (int)mesh.polygonStride; e++) {
      auto it = edgeIterators(e, f);

      const std::vector<EdgeData>& adj = it->second;

      // find adjacent face
      int adjFace = -1;
      for (size_t i = 0; i < adj.size(); i++) {
        if (adj[i].face != (int)f)
          adjFace = adj[i].face;
      }

      // find number of 90 degree rotation steps between faces
      int rot = 0;
      if (adj.size() == 2) {
        int edge0 = 0, edge1 = 0;
        if (adj[0].edge == e) {
          edge0 = adj[0].edge;
          edge1 = adj[1].edge;
        } else if (adj[1].edge == e) {
          edge0 = adj[1].edge;
          edge1 = adj[0].edge;
        }

        rot = (edge0 - edge1 + 2) & 3;
      }

      // pack adjacent face and rotation into 32-bit int
      adjFaces[f * mesh.polygonStride + e] = (rot << ROTATION_SHIFT) | (adjFace & FACE_MASK);
    }
  }
}

void PTexMesh::LoadMeshData(const std::string& meshFile) {
  // Load the meshes
  MeshData originalMesh;
  PLYParse(originalMesh, meshFile);

  ASSERT(originalMesh.polygonStride == 4, "Must be a quad mesh!");

  // Split into sub-meshes
  std::vector<MeshData> splitMeshData;

  if (splitSize > 0.0f) {
    std::cout << "Splitting mesh... ";
    std::cout.flush();
    splitMeshData = SplitMesh(originalMesh, splitSize);
    std::cout << "done" << std::endl;
  } else {
    splitMeshData.emplace_back(std::move(originalMesh));
  }

  // Upload mesh data to GPU
  for (size_t i = 0; i < splitMeshData.size(); i++) {
    std::cout << "\rLoading mesh " << i + 1 << "/" << splitMeshData.size() << "... ";
    std::cout.flush();

    meshes.emplace_back(new Mesh);

    meshes.back()->vbo.Reinitialise(
        pangolin::GlArrayBuffer, splitMeshData[i].vbo.Area(), GL_FLOAT, 4, GL_STATIC_DRAW);
    meshes.back()->vbo.Upload(
        splitMeshData[i].vbo.ptr, splitMeshData[i].vbo.Area() * sizeof(Eigen::Vector4f));
    meshes.back()->ibo.Reinitialise(
        pangolin::GlElementArrayBuffer,
        splitMeshData[i].ibo.Area(),
        GL_UNSIGNED_INT,
        1,
        GL_STATIC_DRAW);
    meshes.back()->ibo.Upload(
        splitMeshData[i].ibo.ptr, splitMeshData[i].ibo.Area() * sizeof(unsigned int));
  }
  std::cout << "\rLoading mesh " << splitMeshData.size() << "/" << splitMeshData.size()
            << "... done" << std::endl;

  std::cout << "Calculating mesh adjacency... ";
  std::cout.flush();

  std::vector<std::vector<uint32_t>> adjFaces(splitMeshData.size());

#pragma omp parallel for
  for (int i = 0; i < splitMeshData.size(); i++) {
    CalculateAdjacency(splitMeshData[i], adjFaces[i]);
  }

  for (size_t i = 0; i < splitMeshData.size(); i++) {
    meshes[i]->abo.Reinitialise(
        pangolin::GlShaderStorageBuffer, adjFaces[i].size(), GL_INT, 1, GL_STATIC_DRAW);
    meshes[i]->abo.Upload(adjFaces[i].data(), sizeof(uint32_t) * adjFaces[i].size());
  }
  std::cout << "done" << std::endl;
}

void PTexMesh::LoadAtlasData(const std::string& atlasFolder) {
  isHdr = false;
  // Upload atlas data to GPU
  for (size_t i = 0; i < meshes.size(); i++) {
    const std::string dxtFile = atlasFolder + "/" + std::to_string(i) + "-color-ptex.dxt1";
    const std::string rgbFile = atlasFolder + "/" + std::to_string(i) + "-color-ptex.rgb";
    const std::string hdrFile = atlasFolder + "/" + std::to_string(i) + "-color-ptex.hdr";

    if (pangolin::FileExists(dxtFile)) {
      const size_t numBytes = std::filesystem::file_size(dxtFile);

      // We know it's square
      const size_t dim = std::sqrt(numBytes * 2);

      meshes[i]->atlas.Reinitialise(
          dim, dim, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, false, 0, GL_RGBA, GL_UNSIGNED_BYTE);
    } else if (pangolin::FileExists(rgbFile)) {
      const size_t numBytes = std::filesystem::file_size(rgbFile);

      // We know it's square
      const size_t dim = std::sqrt(numBytes / 3);

      meshes[i]->atlas.Reinitialise(dim, dim, GL_RGBA8, true, 0, GL_RGB, GL_UNSIGNED_BYTE);
    } else if (pangolin::FileExists(hdrFile)) {
      const size_t numBytes = std::filesystem::file_size(hdrFile);

      // We know it's square
      const size_t dim = std::sqrt(numBytes / 6);

      meshes[i]->atlas.Reinitialise(dim, dim, GL_RGBA16F, false, 0, GL_RGB, GL_HALF_FLOAT);
      isHdr = true;
    } else {
      ASSERT(false, "Can't parse texture filename " + atlasFolder + "/" + std::to_string(i));
    }
  }

  for (size_t i = 0; i < meshes.size(); i++) {
    std::cout << "\rLoading atlas " << i + 1 << "/" << meshes.size() << "... ";
    std::cout.flush();
    const std::string dxtFile = atlasFolder + "/" + std::to_string(i) + "-color-ptex.dxt1";
    const std::string rgbFile = atlasFolder + "/" + std::to_string(i) + "-color-ptex.rgb";
    const std::string hdrFile = atlasFolder + "/" + std::to_string(i) + "-color-ptex.hdr";

    if (pangolin::FileExists(dxtFile)) {
      const size_t numBytes = std::filesystem::file_size(dxtFile);

      // Open file
      FileMemMap fileMap;
      void* mmappedData = fileMap.mapfile(std::string(dxtFile));

      meshes[i]->atlas.Bind();
      glCompressedTexSubImage2D(
          GL_TEXTURE_2D,
          0,
          0,
          0,
          meshes[i]->atlas.width,
          meshes[i]->atlas.height,
          GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
          numBytes,
          mmappedData);
      CheckGlDieOnError();

      fileMap.release();
    } else if (pangolin::FileExists(rgbFile)) {
      const size_t numBytes = std::filesystem::file_size(rgbFile);

      // Open file
      FileMemMap fileMap;
      void* mmappedData = fileMap.mapfile(std::string(rgbFile));

      meshes[i]->atlas.Upload(mmappedData, GL_RGB, GL_UNSIGNED_BYTE);

      fileMap.release();
    } else if (pangolin::FileExists(hdrFile)) {
      const size_t numBytes = std::filesystem::file_size(hdrFile);

      // Open file
      FileMemMap fileMap;
      void* mmappedData = fileMap.mapfile(std::string(hdrFile));

      meshes[i]->atlas.Upload(mmappedData, GL_RGB, GL_HALF_FLOAT);

      fileMap.release();
    } else {
      ASSERT(false, "Can't parse texture filename " + atlasFolder + "/" + std::to_string(i));
    }
  }
  std::cout << "\rLoading atlas " << meshes.size() << "/" << meshes.size() << "... done"
            << std::endl;
}
