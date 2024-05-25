cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/deps.cmake")

project(voxel_raster VERSION 0.1.0)
add_executable(${PROJECT_NAME}
    "src/main.cpp"
    "src/player.cpp"
    "src/renderer/renderer.cpp"
    "src/voxels/voxel_world.cpp"
)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

find_package(daxa CONFIG REQUIRED)
find_package(gvox CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
    daxa::daxa
    gvox::gvox
    glfw
    glm::glm
)
target_include_directories(${PROJECT_NAME} PRIVATE
    "src"
)
