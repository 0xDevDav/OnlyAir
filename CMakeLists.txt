cmake_minimum_required(VERSION 3.20)
project(OnlyAir VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Qt6 setup
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Windows specific
if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX)
    # Use MD runtime for Qt compatibility
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Qt6 - set path hint
if(NOT DEFINED Qt6_DIR)
    set(Qt6_DIR "C:/Qt/6.10.1/msvc2022_64/lib/cmake/Qt6" CACHE PATH "Qt6 CMake directory")
endif()

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGL OpenGLWidgets)

# FFmpeg - expects FFMPEG_DIR to be set
if(NOT DEFINED FFMPEG_DIR)
    set(FFMPEG_DIR "${CMAKE_SOURCE_DIR}/third_party/ffmpeg" CACHE PATH "FFmpeg installation directory")
endif()

# Find FFmpeg libraries
find_path(FFMPEG_INCLUDE_DIR libavcodec/avcodec.h
    PATHS ${FFMPEG_DIR}/include
    NO_DEFAULT_PATH
)

find_library(AVCODEC_LIBRARY avcodec
    PATHS ${FFMPEG_DIR}/lib
    NO_DEFAULT_PATH
)
find_library(AVFORMAT_LIBRARY avformat
    PATHS ${FFMPEG_DIR}/lib
    NO_DEFAULT_PATH
)
find_library(AVUTIL_LIBRARY avutil
    PATHS ${FFMPEG_DIR}/lib
    NO_DEFAULT_PATH
)
find_library(SWSCALE_LIBRARY swscale
    PATHS ${FFMPEG_DIR}/lib
    NO_DEFAULT_PATH
)
find_library(SWRESAMPLE_LIBRARY swresample
    PATHS ${FFMPEG_DIR}/lib
    NO_DEFAULT_PATH
)
find_library(AVFILTER_LIBRARY avfilter
    PATHS ${FFMPEG_DIR}/lib
    NO_DEFAULT_PATH
)

# Shared library (for IPC)
add_library(OnlyAirShared STATIC
    src/shared/SharedMemory.cpp
    src/shared/FrameBuffer.cpp
)
target_include_directories(OnlyAirShared PUBLIC
    ${CMAKE_SOURCE_DIR}/src/shared
)

# Qt UI sources
set(QT_SOURCES
    src/app/qt/MainWindow.cpp
    src/app/qt/VideoPreviewWidget.cpp
    src/app/qt/ScenePanel.cpp
    src/app/qt/PlaybackControls.cpp
)

# Windows resource file (icon for .exe)
set(WIN_RESOURCES ${CMAKE_SOURCE_DIR}/resources/OnlyAir.rc)

# Qt resource file (icon embedded in app)
set(QT_RESOURCES ${CMAKE_SOURCE_DIR}/resources/resources.qrc)

# Main application
add_executable(OnlyAir WIN32
    src/app/main.cpp
    src/app/Application.cpp
    src/app/VideoPlayer.cpp
    src/app/AudioPlayer.cpp
    src/app/SceneManager.cpp
    src/app/Translator.cpp
    ${QT_SOURCES}
    ${WIN_RESOURCES}
    ${QT_RESOURCES}
)

# Softcam library path (Virtual Camera)
set(SOFTCAM_DIR ${CMAKE_SOURCE_DIR}/third_party/softcam)
set(SOFTCAM_INCLUDE_DIR ${SOFTCAM_DIR}/dist/include)
set(SOFTCAM_LIB ${SOFTCAM_DIR}/dist/lib/x64/softcam.lib)

target_include_directories(OnlyAir PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/app
    ${CMAKE_SOURCE_DIR}/third_party
    ${FFMPEG_INCLUDE_DIR}
    ${SOFTCAM_INCLUDE_DIR}
)

target_link_libraries(OnlyAir PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    ${AVCODEC_LIBRARY}
    ${AVFORMAT_LIBRARY}
    ${AVUTIL_LIBRARY}
    ${SWSCALE_LIBRARY}
    ${SWRESAMPLE_LIBRARY}
    ${AVFILTER_LIBRARY}
    ${SOFTCAM_LIB}
    winmm
    ole32
    oleaut32
    uuid
    opengl32
)

# Install targets
install(TARGETS OnlyAir
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
)

# Copy FFmpeg DLLs to output directory
if(EXISTS "${FFMPEG_DIR}/bin")
    file(GLOB FFMPEG_DLLS "${FFMPEG_DIR}/bin/*.dll")
    foreach(DLL ${FFMPEG_DLLS})
        add_custom_command(TARGET OnlyAir POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DLL}"
            $<TARGET_FILE_DIR:OnlyAir>
        )
    endforeach()
endif()

# Copy Qt DLLs to output directory (for development)
add_custom_command(TARGET OnlyAir POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:Qt6::Core>
    $<TARGET_FILE:Qt6::Widgets>
    $<TARGET_FILE:Qt6::Gui>
    $<TARGET_FILE:Qt6::OpenGL>
    $<TARGET_FILE:Qt6::OpenGLWidgets>
    $<TARGET_FILE_DIR:OnlyAir>
)

message(STATUS "Qt6 found: ${Qt6_DIR}")
message(STATUS "FFmpeg include dir: ${FFMPEG_INCLUDE_DIR}")
message(STATUS "FFmpeg libraries: ${AVCODEC_LIBRARY}")
message(STATUS "Softcam library: ${SOFTCAM_LIB}")
