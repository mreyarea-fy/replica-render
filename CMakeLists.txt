# Copyright (c) Facebook, Inc. and its affiliates. All Rights Reserved
cmake_minimum_required(VERSION 3.9.0)

project(Replica)

# set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake/")

# set(THIRD_PARTY "${CMAKE_CURRENT_LIST_DIR}/3rdparty/")

set (CMAKE_CXX_STANDARD 17)
add_definitions(-DEIGEN_NO_DEBUG)

# OpenMP
find_package(OpenMP)
if (OPENMP_FOUND)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

if(WIN32)
    message("The platform is Windows.")
    set(CMAKE_DEBUG_POSTFIX "d" CACHE STRING "Set debug library postfix")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")

    # include 3rd libraries path
    #############################
    # DEPENDENCIES DIR
    #############################
    set(Eigen3_DIR "D:/libraries_windows/eigen/eigen-3.3.9-bin/share/eigen3/cmake")
    set(gflags_DIR "D:/libraries_windows/glog/glog_with_gflags-0.5.0-msvc14.0-x64-dll/gflags-2.2.2-bin/lib/cmake/gflags/")
    set(glog_DIR "D:/libraries_windows/glog/glog_with_gflags-0.5.0-msvc14.0-x64-dll/glog-0.5.0-bin/lib/cmake/glog/")
    set(GLEW_DIR "D:/libraries_windows/glew/glew-2.1.0-bin/lib/cmake/glew/")
    set(Pangolin_DIR "D:/libraries_windows/Pangolin/Pangolin-bin/lib/cmake/Pangolin")

    #############################
    # SET RUN-TIME ENV FOR MSVC
    #############################
    unset(RUNTIMT_ENV_PATH_LIST)
    unset(RUNTIMT_ENV_PATH_STR)
    unset(RUNTIMT_ENV_PATH)
    SET(RUNTIMT_ENV_PATH_LIST "PATH=%PATH%")
    LIST(APPEND RUNTIMT_ENV_PATH_LIST "D:/libraries_windows/glog/glog_with_gflags-0.5.0-msvc14.0-x64-dll/gflags-2.2.2-bin/bin/")
    LIST(APPEND RUNTIMT_ENV_PATH_LIST "D:/libraries_windows/glog/glog_with_gflags-0.5.0-msvc14.0-x64-dll/glog-0.5.0-bin/bin/")
    LIST(APPEND RUNTIMT_ENV_PATH_LIST "D:/libraries_windows/glew/glew-2.1.0-bin/bin/")
    LIST(APPEND RUNTIMT_ENV_PATH_LIST "D:/libraries_windows/Pangolin/Pangolin-bin/bin")
    LIST(APPEND RUNTIMT_ENV_PATH_LIST "D:/workspace_windows/360_opticalflow/Rendering360OpticalFlow-replica/3rdparty/linux_sort_dll/")
    string(JOIN ";" RUNTIMT_ENV_PATH_STR ${RUNTIMT_ENV_PATH_LIST})
    #string(CONCAT ";" RUNTIMT_ENV_PATH_STR ${RUNTIMT_ENV_PATH_LIST})
    set(RUNTIMT_ENV_PATH ${RUNTIMT_ENV_PATH_STR} CACHE STRING "vs runtime env" FORCE)
    MESSAGE(STATUS "DEBUG: RUNTIMT_ENV_PATH_STR=${RUNTIMT_ENV_PATH}")

    #set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake/")
    # set(THIRD_PARTY "${CMAKE_CURRENT_LIST_DIR}/3rdparty/")
    # set(Eigen_INCLUDE_DIRS "${THIRD_PARTY}/eigen") 
    # set(Pangolin_INCLUDE_DIRS "${THIRD_PARTY}/Pangolin/build/src/include")
    # list(APPEND Pangolin_INCLUDE_DIRS "${THIRD_PARTY}/Pangolin/include") 
    # set(Pangolin_LIBRARIES "${THIRD_PARTY}/Pangolin/build/src/libpangolin.so") 

    # find_package(dl REQUIRED)
    # Pangolin's dependencies
    set(ZLIB_INCLUDE_DIR "D:/libraries_windows/Pangolin/Pangolin-bin/include")
    set(ZLIB_LIBRARY_DEBUG  "D:/libraries_windows/Pangolin/Pangolin-bin/lib/zlibstaticd.lib")
    set(ZLIB_LIBRARY_RELEASE   "D:/libraries_windows/Pangolin/Pangolin-bin/lib/zlibstatic.lib")
    set(PNG_PNG_INCLUDE_DIR  "D:/libraries_windows/Pangolin/Pangolin-bin/include/libpng16")
    set(PNG_LIBRARY_DEBUG  "D:/libraries_windows/Pangolin/Pangolin-bin/lib/libpng16_staticd.lib")
    set(PNG_LIBRARY_RELEASE  "D:/libraries_windows/Pangolin/Pangolin-bin/lib/libpng16_static.lib")
    set(JPEG_LIBRARY  "D:/libraries_windows/Pangolin/Pangolin-bin/lib/jpeg.lib")

    set(Pangolin_INCLUDE_DIRS "D:/libraries_windows/Pangolin/Pangolin-bin/include")
    set(Pangolin_LIBRARIES "D:/libraries_windows/Pangolin/Pangolin-bin/lib/pangolin.lib")

    set(SortLinux_INCLUDE_DIR "D:/workspace_windows/360_opticalflow/Rendering360OpticalFlow-replica/3rdparty/linux_sort_dll/")
    set(SortLinux_LIBRARIES "D:/workspace_windows/360_opticalflow/Rendering360OpticalFlow-replica/3rdparty/linux_sort_dll/sort_linux.lib")
endif()

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
    message("The platform is Linux.")
endif()

#if(LINUX)
#    set(GLog_DIR "/mnt/sda1/libraries_linux_ubuntu_20_04/glog/glog-0.5.0-bin/lib/cmake/glog/")
#    set(Pangolin_DIR "/mnt/sda1/libraries_linux_ubuntu_20_04/pangolin/Pangolin-0.6-bin/lib/cmake/Pangolin/")
#endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(glog REQUIRED libglog)
find_package(X11 REQUIRED)
find_package(Eigen3 REQUIRED NO_MODULE)
find_package(GLEW REQUIRED)
find_package(Pangolin REQUIRED)
#find_package(glog REQUIRED)
find_package(gflags REQUIRED)

add_subdirectory("./ReplicaSDK")
