find_package(OpenCV REQUIRED)

add_definitions(--std=c++11)
include_directories(${OpenCV_INCLUDE_DIRS})

add_executable(feye feye.cpp)
target_link_libraries(feye  ${OpenCV_LIBRARIES})
