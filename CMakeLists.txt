cmake_minimum_required(VERSION 3.15)
project(RemoteImGuiServer VERSION 1.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# FetchContent cache configuration
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/.fc_cache" CACHE PATH "" FORCE)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE) 

include(FetchContent)

# Download nlohmann_json
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(json)

# Download ZLIB
FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG        v1.3.1
)
set(ZLIB_BUILD_EXAMPLES OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(zlib)
FetchContent_GetProperties(zlib)

# Set the exact variables that FindZLIB.cmake checks for
set(ZLIB_INCLUDE_DIR "${zlib_SOURCE_DIR};${zlib_BINARY_DIR}" CACHE INTERNAL "")
set(ZLIB_INCLUDE_DIRS "${ZLIB_INCLUDE_DIR}" CACHE INTERNAL "")

if(TARGET zlibstatic)
    set(ZLIB_LIBRARY zlibstatic CACHE INTERNAL "")
    set(ZLIB_LIBRARIES zlibstatic CACHE INTERNAL "")
    set(ZLIB_FOUND TRUE CACHE INTERNAL "")
    
    # Also create the alias target just in case
    if(NOT TARGET ZLIB::ZLIB)
        add_library(ZLIB::ZLIB ALIAS zlibstatic)
    endif()
endif()

set(CMAKE_SKIP_INSTALL_RULES ON)

# Download IXWebSocket
FetchContent_Declare(
    ixwebsocket
    GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
    GIT_TAG        v12.0.1 
)
set(USE_TLS OFF CACHE INTERNAL "")
set(BUILD_HTTPD ON CACHE INTERNAL "")
set(USE_ZLIB ON CACHE INTERNAL "")
FetchContent_MakeAvailable(ixwebsocket)

# Download Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.9b
)
FetchContent_MakeAvailable(imgui)

# Download ImPlot
FetchContent_Declare(
    implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG        v1.0
)
FetchContent_MakeAvailable(implot)


# Create a single static library for UI and plotting
add_library(imgui_implot STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
    
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
    ${implot_SOURCE_DIR}/implot_demo.cpp
)

# Set up include directories for the UI library (Target name fixed to imgui_implot)
target_include_directories(imgui_implot PUBLIC 
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/misc/cpp
    ${implot_SOURCE_DIR}
)


# Main App setup
find_package(Threads REQUIRED)

set(SOURCES
    server.cpp
    client_session.cpp
    server.h
    client_session.h
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link everything
target_link_libraries(${PROJECT_NAME} PRIVATE
    nlohmann_json::nlohmann_json
    ixwebsocket
    imgui_implot
    Threads::Threads
)

# Platform-specific system libraries
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 mswsock)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_link_libraries(${PROJECT_NAME} PRIVATE m dl)
endif()
