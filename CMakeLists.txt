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
    detectors/ball_tracking.cpp  # Mới
    detectors/line_detector.cpp  # Mới
    utils/geometry.cpp
    utils/kalman.cpp
)

add_executable(run_app ${SOURCES})
target_link_libraries(run_app ${OpenCV_LIBS})