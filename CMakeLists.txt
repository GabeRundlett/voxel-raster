cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/deps.cmake")

enable_language(ISPC)
project(voxel_raster VERSION 0.1.0)
add_executable(${PROJECT_NAME}
    "src/main.cpp"
    "src/player.cpp"
    "src/renderer/renderer.cpp"
    "src/voxels/voxel_world.cpp"
    "src/utilities/thread_pool.cpp"
    "src/voxels/generation.ispc"
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

if (UNIX)
    if(NOT ISPC_ARCH)
        execute_process( COMMAND sh "-c" "uname -m | sed -e s/x86_64/x86/ -e s/amd64/x86/ -e s/i686/x86/ -e s/arm64/aarch64/ -e s/arm.*/arm/ -e s/sa110/arm/" OUTPUT_VARIABLE ARCH)
        string(STRIP ${ARCH} ARCH)
        execute_process( COMMAND getconf LONG_BIT OUTPUT_VARIABLE ARCH_BIT)
        string(STRIP ${ARCH_BIT} arch_bit)
        if ("${ARCH}" STREQUAL "x86")
            if (${arch_bit} EQUAL 32)
                set(ispc_arch "x86")
            else()
                set(ispc_arch "x86-64")
            endif()
        elseif ("${ARCH}" STREQUAL "arm")
            set(ispc_arch "arm")
        elseif ("${ARCH}" STREQUAL "aarch64")
            set(ispc_arch "aarch64")
        endif()
    endif()
    set(ISPC_ARCH "${ispc_arch}" CACHE STRING "ISPC CPU ARCH")
    set(ISPC_ARCH_BIT "${arch_bit}" CACHE STRING "ISPC CPU BIT")
else()
    if(NOT ISPC_ARCH)
        set(ispc_arch "x86")
        if (CMAKE_SIZEOF_VOID_P EQUAL 8 )
            set(ispc_arch "x86-64")
        endif()
    endif()
    set(ISPC_ARCH "${ispc_arch}" CACHE STRING "ISPC CPU ARCH")
    set(ISPC_ARCH_BIT "${arch_bit}" CACHE STRING "ISPC CPU BIT")
endif()

set(ISPC_IA_TARGETS "sse2-i32x4,sse4-i32x8,avx1-i32x16,avx2-i32x16,avx512knl-x16,avx512skx-x16" CACHE STRING "ISPC IA targets")
set(ISPC_ARM_TARGETS "neon" CACHE STRING "ISPC ARM targets")
set(ISPC_FLAGS "--opt=fast-math")
if ("${ISPC_ARCH}" MATCHES "x86")
    string(REPLACE "," ";" ISPC_TARGETS ${ISPC_IA_TARGETS})
elseif ("${ISPC_ARCH}" STREQUAL "arm" OR "${ISPC_ARCH}" STREQUAL "aarch64")
    string(REPLACE "," ";" ISPC_TARGETS ${ISPC_ARM_TARGETS})
else()
    message(FATAL_ERROR "Unknown architecture ${ISPC_ARCH}")
endif()

set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET ${PROJECT_NAME} PROPERTY ISPC_INSTRUCTION_SETS "${ISPC_TARGETS}")
target_compile_options(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:ISPC>:--arch=${ISPC_ARCH}>)
target_compile_options(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:ISPC>:${ISPC_FLAGS}>)
# target_compile_options(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-Wl,/fp:fast -Wl,/Oi>)
# target_compile_options(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-march=native>)
