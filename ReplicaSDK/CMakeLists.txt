# Copyright (c) Facebook, Inc. and its affiliates. All Rights Reserved
cmake_minimum_required(VERSION 3.9.0)

project(ReplicaSDK)

#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/../cmake")

set(PTEX_SHADER_DIR "${CMAKE_CURRENT_LIST_DIR}/shaders" CACHE PATH "Where the shaders live")

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -msse2 -msse3 -Wall -std=c++11 -fopenmp -DSHADER_DIR=${PTEX_SHADER_DIR}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSHADER_DIR=${PTEX_SHADER_DIR}")


#######   ReplicaRenderer   #######
file(GLOB srcs_ptex ptex/*.cpp)
file(GLOB hdrs include/*.h)

add_library(ptex STATIC 
            ${srcs_ptex}
            ${hdrs}
)

target_link_libraries(ptex PUBLIC
                      ${Pangolin_LIBRARIES}
                      ${SortLinux_LIBRARIES}
                      GLEW::glew
                      Eigen3::Eigen
)

target_include_directories(ptex PUBLIC
        "./include"
            ${PNG_PNG_INCLUDE_DIR}
            ${ZLIB_INCLUDE_DIR}
            ${Pangolin_INCLUDE_DIRS}
            ${SortLinux_INCLUDE_DIR}
            ${CMAKE_CURRENT_LIST_DIR}
            GL
)

#######   ReplicaViewer   #######
add_executable(ReplicaViewer src/viewer.cpp )
set_target_properties(ReplicaViewer PROPERTIES VS_DEBUGGER_ENVIRONMENT "${RUNTIMT_ENV_PATH}")
target_link_libraries(ReplicaViewer PUBLIC
                      ptex
)

#######   ReplicaRenderer   #######
add_executable(ReplicaRenderer src/render.cpp)
set_target_properties(ReplicaRenderer PROPERTIES VS_DEBUGGER_ENVIRONMENT "${RUNTIMT_ENV_PATH}")
target_link_libraries(ReplicaRenderer PUBLIC
                      ptex
                      ${CMAKE_DL_LIBS}
)

#######   ReplicaRendererCubemap   #######
add_executable(ReplicaRendererCubemap src/renderCubemap.cpp)
set_target_properties(ReplicaRendererCubemap PROPERTIES VS_DEBUGGER_ENVIRONMENT "${RUNTIMT_ENV_PATH}")
target_link_libraries(ReplicaRendererCubemap PUBLIC
                    gflags
                    ${glog_LIBRARIES}
		    ptex
                    ${CMAKE_DL_LIBS}
)
set(MODULE_NAME ReplicaRendererCubemap)
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # generate *.pdb file for Visual studio debug
    message("Set ${MODULE_NAME} to generate *.pdb for MSVC debugging.")
    target_compile_options(${MODULE_NAME} PRIVATE $<$<CONFIG:Release>:/Zi>)
    target_link_options(${MODULE_NAME} PRIVATE $<$<CONFIG:Release>:/DEBUG>)
    target_link_options(${MODULE_NAME} PRIVATE $<$<CONFIG:Release>:/OPT:REF>)
    target_link_options(${MODULE_NAME} PRIVATE $<$<CONFIG:Release>:/OPT:ICF>)
    # DLL export all symbols
    set_target_properties(${MODULE_NAME} PROPERTIES ENABLE_EXPORTS 1)
    set_target_properties(${MODULE_NAME} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS 1)
endif()

#######   ReplicaRendererPanorama   #######
add_executable(ReplicaRendererPanorama src/renderPanorama.cpp)
set_target_properties(ReplicaRendererPanorama PROPERTIES VS_DEBUGGER_ENVIRONMENT "${RUNTIMT_ENV_PATH}")
target_link_libraries(ReplicaRendererPanorama PUBLIC
                    gflags
                    ${glog_LIBRARIES}
		    ptex
                    ${CMAKE_DL_LIBS}
${X11_LIBRARIES}
)

#######   openGL_version   #######
add_executable(openGL_version src/openGL_version.cpp)
set_target_properties(openGL_version PROPERTIES VS_DEBUGGER_ENVIRONMENT "${RUNTIMT_ENV_PATH}")
target_link_libraries(openGL_version PUBLIC
                    gflags
		    ${glog_LIBRARIES}
                    ptex
                    ${CMAKE_DL_LIBS}
		    ${X11_LIBRARIES}
)
