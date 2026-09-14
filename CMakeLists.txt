cmake_minimum_required(VERSION 3.15)
project(RemoteImGuiServer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/.fc_cache" CACHE PATH "" FORCE)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE) 

# Include FetchContent module
include(FetchContent)

# Download nlohmann_json
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(json)

# Download IXWebSocket
FetchContent_Declare(
    ixwebsocket
    GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
    GIT_TAG        v11.4.5 
)
set(USE_TLS OFF CACHE INTERNAL "")
set(BUILD_HTTPD ON CACHE INTERNAL "")
set(USE_ZLIB OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(ixwebsocket)

# Download Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.9b
)
FetchContent_MakeAvailable(imgui)

# Retrieve the source path of the downloaded ImGui repository
FetchContent_GetProperties(imgui SOURCE_DIR IMGUI_DIR)

# Create a standalone static library for ImGui
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
)

# Bind include directories directly to the target 'imgui' library
target_include_directories(imgui PUBLIC 
    ${IMGUI_DIR}
    ${IMGUI_DIR}/misc/cpp
)

# Main App setup
find_package(Threads REQUIRED)

# Only your own server logic files go here
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
    imgui
    Threads::Threads
)

if(WIN32)
    # Windows system networking libraries
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 mswsock)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_link_libraries(${PROJECT_NAME} PRIVATE m dl)
endif()
