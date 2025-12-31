cmake_minimum_required(VERSION 3.10)
project(PickleballAI)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenCV REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}) 
include_directories(${OpenCV_INCLUDE_DIRS})

# --- THÊM FILE MỚI VÀO ĐÂY ---
set(SOURCES
    main.cpp
    detectors/ball_detector.cpp
    detectors/ball_tracking.cpp
    detectors/line_detector.cpp
    utils/geometry.cpp
    utils/kalman.cpp
    utils/vlc_reader.cpp  # VLC video reader
)

# Tìm VLC
if(WIN32)
    # Trên Windows, VLC thường được cài ở Program Files
    find_path(VLC_INCLUDE_DIR
        NAMES vlc/vlc.h
        PATHS
            "C:/Program Files/VideoLAN/VLC/sdk/include"
            "C:/Program Files (x86)/VideoLAN/VLC/sdk/include"
            "$ENV{ProgramFiles}/VideoLAN/VLC/sdk/include"
            "$ENV{ProgramFiles(x86)}/VideoLAN/VLC/sdk/include"
    )
    
    find_library(VLC_LIBRARY
        NAMES libvlc libvlc.lib
        PATHS
            "C:/Program Files/VideoLAN/VLC/sdk/lib"
            "C:/Program Files (x86)/VideoLAN/VLC/sdk/lib"
            "$ENV{ProgramFiles}/VideoLAN/VLC/sdk/lib"
            "$ENV{ProgramFiles(x86)}/VideoLAN/VLC/sdk/lib"
    )
else()
    # Trên Linux, sử dụng pkg-config nếu có
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(VLC QUIET libvlc)
        if(VLC_FOUND)
            set(VLC_INCLUDE_DIR ${VLC_INCLUDE_DIRS})
            set(VLC_LIBRARY ${VLC_LIBRARIES})
        endif()
    endif()
    
    # Fallback: tìm thủ công
    if(NOT VLC_INCLUDE_DIR)
        find_path(VLC_INCLUDE_DIR
            NAMES vlc/vlc.h
            PATHS
                /usr/include
                /usr/local/include
                /opt/vlc/include
        )
    endif()
    
    if(NOT VLC_LIBRARY)
        find_library(VLC_LIBRARY
            NAMES vlc libvlc
            PATHS
                /usr/lib
                /usr/local/lib
                /opt/vlc/lib
                /usr/lib/x86_64-linux-gnu
        )
    endif()
endif()

if(VLC_INCLUDE_DIR AND VLC_LIBRARY)
    message(STATUS "Found VLC: ${VLC_LIBRARY}")
    include_directories(${VLC_INCLUDE_DIR})
else()
    message(FATAL_ERROR "VLC not found! Please install libvlc-dev (Linux) or VLC SDK (Windows)")
endif()

add_executable(run_app ${SOURCES})
target_link_libraries(run_app ${OpenCV_LIBS} ${VLC_LIBRARY})