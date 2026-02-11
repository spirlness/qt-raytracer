cmake_minimum_required(VERSION 3.16)
project(Qt-CMake-HelloWorld)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

option(ENABLE_CUDA "Enable CUDA path tracing backend" OFF)
option(ENABLE_VULKAN_COMPUTE "Enable Vulkan compute path tracing backend" ON)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

if(ENABLE_CUDA)
    enable_language(CUDA)
endif()

find_package(Qt6 REQUIRED COMPONENTS Quick OpenGL)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_executable(helloworld
    main.cpp
    GpuPathTracer.cpp GpuPathTracer.h
    CudaPathTracer.cpp CudaPathTracer.h
    VulkanPathTracer.cpp VulkanPathTracer.h
    RayTracer.h
    RayTracerFboItem.cpp RayTracerFboItem.h
)

if(ENABLE_CUDA)
    target_sources(helloworld PRIVATE CudaPathTracerKernel.cu)
    target_compile_definitions(helloworld PRIVATE ENABLE_CUDA_BACKEND=1)
    set_target_properties(helloworld PROPERTIES CUDA_SEPARABLE_COMPILATION ON)
endif()

if(ENABLE_VULKAN_COMPUTE)
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        target_compile_definitions(helloworld PRIVATE ENABLE_VULKAN_COMPUTE=1)
        target_link_libraries(helloworld Vulkan::Vulkan)

        set(VULKAN_COMPUTE_SHADER "${CMAKE_CURRENT_SOURCE_DIR}/shaders/pathtrace_vulkan.comp")
        set(VULKAN_COMPUTE_SPV "${CMAKE_CURRENT_BINARY_DIR}/pathtrace_vulkan.comp.spv")
        set(VULKAN_COMPUTE_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/VulkanPathTracerSpv.h")
        set(SPV_TO_HEADER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/scripts/spv_to_header.py")

        find_program(GLSLANG_VALIDATOR_EXECUTABLE glslangValidator
            HINTS
                "$ENV{VULKAN_SDK}/Bin"
                "$ENV{VULKAN_SDK}/bin"
        )

        if(NOT GLSLANG_VALIDATOR_EXECUTABLE)
            file(GLOB VULKAN_GLSLANG_HINTS "C:/VulkanSDK/*/Bin")
            find_program(GLSLANG_VALIDATOR_EXECUTABLE glslangValidator HINTS ${VULKAN_GLSLANG_HINTS})
        endif()

        if(GLSLANG_VALIDATOR_EXECUTABLE)
            add_custom_command(
                OUTPUT "${VULKAN_COMPUTE_HEADER}"
                COMMAND "${GLSLANG_VALIDATOR_EXECUTABLE}" -V "${VULKAN_COMPUTE_SHADER}" -o "${VULKAN_COMPUTE_SPV}"
                COMMAND "${Python3_EXECUTABLE}" "${SPV_TO_HEADER_SCRIPT}" --input "${VULKAN_COMPUTE_SPV}" --output "${VULKAN_COMPUTE_HEADER}" --symbol kPathtraceVulkanSpv
                DEPENDS "${VULKAN_COMPUTE_SHADER}" "${SPV_TO_HEADER_SCRIPT}"
                COMMENT "Regenerating VulkanPathTracerSpv.h from GLSL"
                VERBATIM
            )

            add_custom_target(regen_spv DEPENDS "${VULKAN_COMPUTE_HEADER}")
        else()
            add_custom_target(regen_spv
                COMMAND ${CMAKE_COMMAND} -E echo "glslangValidator not found. Install Vulkan SDK or set VULKAN_SDK."
            )
        endif()
    endif()
endif()

qt_add_resources(helloworld "qml_resources"
    PREFIX "/"
    FILES
        qml/BlurLayer.qml
        qml/Main.qml
)

target_link_libraries(helloworld Qt6::Quick Qt6::OpenGL)

if(WIN32)
    get_filename_component(TOOLCHAIN_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${TOOLCHAIN_BIN_DIR}")

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET helloworld POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --release "$<TARGET_FILE:helloworld>"
            COMMENT "Deploying Qt runtime for helloworld"
        )
    endif()

    set(MINGW_RUNTIME_DLLS
        "libgcc_s_seh-1.dll"
        "libstdc++-6.dll"
        "libwinpthread-1.dll"
        "libb2-1.dll"
        "libdouble-conversion.dll"
        "libicuin78.dll"
        "libicuuc78.dll"
        "libpcre2-16-0.dll"
        "zlib1.dll"
        "libzstd.dll"
    )

    foreach(RUNTIME_DLL IN LISTS MINGW_RUNTIME_DLLS)
        set(RUNTIME_DLL_PATH "${TOOLCHAIN_BIN_DIR}/${RUNTIME_DLL}")
        if(EXISTS "${RUNTIME_DLL_PATH}")
            add_custom_command(TARGET helloworld POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${RUNTIME_DLL_PATH}"
                    "$<TARGET_FILE_DIR:helloworld>"
                COMMENT "Copying ${RUNTIME_DLL}"
            )
        endif()
    endforeach()
endif()

enable_testing()
include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    URL_HASH SHA256=1F357C27CA988C3F7C6B4BF68A9395005AC6761F034046E9DDE0896E3ABA00E4
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(raytracer_tests
    tests/unit/Vec3Tests.cpp
    tests/unit/RayTests.cpp
    tests/unit/SphereTests.cpp
    tests/unit/HitableListTests.cpp
)

target_include_directories(raytracer_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(raytracer_tests PRIVATE gtest_main)

include(GoogleTest)
gtest_discover_tests(raytracer_tests)
