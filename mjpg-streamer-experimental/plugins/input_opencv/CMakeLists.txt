
# TODO: which components do I need?
# To fix the error: "undefined symbol: _ZN2cv12VideoCaptureC1Ev"
find_package(OpenCV COMPONENTS core imgproc highgui videoio)

MJPG_STREAMER_PLUGIN_OPTION(input_opencv "OpenCV input plugin"
                            ONLYIF OpenCV_FOUND)

#
# To Install OpenCV on RPI
# https://github.com/jabelone/OpenCV-for-Pi
#


FIND_PATH(FREENECT_INCLUDE_DIR libfreenect.h
  /usr/include/libfreenect
  /usr/local/include
  /usr/local/include/libfreenect
  /usr/include
)

FIND_LIBRARY(FREENECT_freenect_LIBRARY freenect
  /usr/local/lib
  /usr/lib
)

MARK_AS_ADVANCED(
  FREENECT_INCLUDE_DIR
  FREENECT_freenect_LIBRARY)

SET( FREENECT_FOUND "NO" )
IF(FREENECT_INCLUDE_DIR)
  IF(FREENECT_freenect_LIBRARY)
    SET( FREENECT_FOUND "YES" )
    SET( FREENECT_LIBRARIES
      ${FREENECT_freenect_LIBRARY})
  ENDIF(FREENECT_freenect_LIBRARY)
ENDIF(FREENECT_INCLUDE_DIR)

IF(FREENECT_FOUND)
  MESSAGE(STATUS "Found freenect library")
ELSE(FREENECT_FOUND)
  IF(FREENECT_FIND_REQUIRED)
    MESSAGE(FATAL_ERROR "Could not find libfreenect 
-- please give some paths to CMake or make sure libfreenect is installed in your system")
  ELSE(FREENECT_FIND_REQUIRED)
    MESSAGE(STATUS "Could not find libfreenect 
-- please give some paths to CMake or make sure libfreenect is installed in your system")
  ENDIF(FREENECT_FIND_REQUIRED)
ENDIF(FREENECT_FOUND)


#find_package(libfreenect REQUIRED)

if (PLUGIN_INPUT_OPENCV)
    include_directories(${OpenCV_INCLUDE_DIRS})
    include_directories("/usr/include/libusb-1.0/")
    
    MJPG_STREAMER_PLUGIN_COMPILE(input_opencv input_opencv.cpp)
    
   


    target_link_libraries(input_opencv ${OpenCV_LIBS}
                                       ${FREENECT_LIBRARIES})

    
    add_subdirectory(filters/cvfilter_cpp)
    add_subdirectory(filters/cvfilter_py)
    
endif()


