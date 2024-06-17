cmake_minimum_required(VERSION 3.21)

set(USE_ISPC 1)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/deps.cmake")
if (USE_ISPC)
    include("${CMAKE_CURRENT_LIST_DIR}/cmake/ispc.cmake")
endif()

project(voxel_raster VERSION 0.1.0)
add_executable(${PROJECT_NAME}
    "src/main.cpp"
    "src/player.cpp"
    "src/audio.cpp"
    "src/renderer/renderer.cpp"
    "src/voxels/voxel_world.cpp"
    "src/utilities/thread_pool.cpp"
    "src/utilities/ispc_instrument.cpp"
    "src/voxels/generation/generation.cpp"
)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

find_package(daxa CONFIG REQUIRED)
find_package(gvox CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(RtAudio CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
    daxa::daxa
    gvox::gvox
    glfw
    glm::glm
    RtAudio::rtaudio
)

target_include_directories(${PROJECT_NAME} PRIVATE
    "src"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE USE_ISPC=${USE_ISPC})
if (USE_ISPC)
    target_sources(${PROJECT_NAME} PRIVATE
        "src/voxels/generation/generation.ispc"
    )
    enable_ispc(${PROJECT_NAME})
endif()
