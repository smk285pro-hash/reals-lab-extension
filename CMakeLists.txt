cmake_minimum_required(VERSION 3.24)
project(reals_lab LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(REALS_BUILD_APP       "Build the standalone desktop app" ON)
option(REALS_BUILD_EXTENSION "Build the REAPER extension (needs REAPER SDK)" OFF)
option(REALS_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

if (MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /FS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if (REALS_WARNINGS_AS_ERRORS)
    if (MSVC)
        add_compile_options(/WX)
    else()
        add_compile_options(-Werror)
    endif()
endif()

include(FetchContent)

# ---- Dependencies -----------------------------------------------------------
# nlohmann/json: JSON parsing (header-only)
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3)
FetchContent_MakeAvailable(json)

# miniaudio: playback / capture / decoding (public domain, single header)
FetchContent_Declare(miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG        0.11.21)
FetchContent_MakeAvailable(miniaudio)

# libcurl: networking (system package: vcpkg on Win / brew / apt)
# Optional at Phase 0 — becomes REQUIRED when the net module lands (Phase 1+).
find_package(CURL QUIET)
if (CURL_FOUND)
    message(STATUS "libcurl found — net module enabled")
else()
    message(WARNING "libcurl NOT found — building core without net for now")
endif()

# ---- SoundTouch DSP Library --------------------------------------------------
file(GLOB SOUNDTOUCH_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/libs/soundtouch/*.cpp)
add_library(soundtouch STATIC ${SOUNDTOUCH_SOURCES})
target_include_directories(soundtouch PUBLIC ${CMAKE_CURRENT_LIST_DIR}/libs/soundtouch)
if (MSVC)
    target_compile_definitions(soundtouch PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# ---- SQLite3 Embedded Database ------------------------------------------------
add_library(sqlite3 STATIC ${CMAKE_CURRENT_LIST_DIR}/libs/sqlite3/sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_LIST_DIR}/libs/sqlite3)
target_compile_definitions(sqlite3 PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_ENABLE_FTS5=1
    SQLITE_ENABLE_JSON1=1
    SQLITE_ENABLE_RTREE=1
    SQLITE_DEFAULT_MEMSTATUS=0
    _CRT_SECURE_NO_WARNINGS
)
if (MSVC)
    target_compile_options(sqlite3 PRIVATE /W3)
endif()

# ---- Core library (no UI, no host deps) --------------------------------------
file(GLOB_RECURSE REALS_CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/core/src/*.cpp)
set(SQLITE3_SOURCES ${CMAKE_CURRENT_LIST_DIR}/libs/sqlite3/sqlite3.c)

add_library(reals_core STATIC ${REALS_CORE_SOURCES} ${SQLITE3_SOURCES})
target_include_directories(reals_core
    PUBLIC  ${CMAKE_CURRENT_LIST_DIR}/core/include
    PRIVATE ${miniaudio_SOURCE_DIR}
            ${CMAKE_CURRENT_LIST_DIR}/libs/soundtouch
            ${CMAKE_CURRENT_LIST_DIR}/libs/sqlite3)
target_link_libraries(reals_core PUBLIC soundtouch nlohmann_json::nlohmann_json)
if (MSVC)
    target_compile_definitions(reals_core PRIVATE _CRT_SECURE_NO_WARNINGS)
    if (WIN32)
        target_link_libraries(reals_core PUBLIC winhttp)
    endif()
endif()
if (CURL_FOUND)
    target_link_libraries(reals_core PUBLIC CURL::libcurl)
    target_compile_definitions(reals_core PUBLIC REALS_HAVE_CURL=1)
endif()

# ---- Bridge (JS <-> C++ dispatcher, shell-agnostic) ---------------------------
add_library(reals_bridge STATIC
    bridge/src/Bridge.cpp)
target_include_directories(reals_bridge
    PUBLIC  ${CMAKE_CURRENT_LIST_DIR}/bridge/include
    PRIVATE ${miniaudio_SOURCE_DIR})
target_link_libraries(reals_bridge PUBLIC reals_core)

# ---- Legacy ImGui UI (deprecated — kept for reference until parity) ----------
if (REALS_BUILD_APP)
    # The ImGui app shell was removed after the WebView2 pivot (P1.5). Only
    # build it when the sources actually exist, so the option stays harmless.
    if (NOT EXISTS ${CMAKE_CURRENT_LIST_DIR}/app/main.cpp)
        message(STATUS "app/main.cpp not found — legacy ImGui app skipped (WebView2 pivot)")
    else()
    # glfw: windowing for the standalone app shell
    set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4)
    FetchContent_MakeAvailable(glfw)

    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.91.9-docking)
    FetchContent_MakeAvailable(imgui)

    add_library(reals_imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp)
    target_include_directories(reals_imgui PUBLIC ${imgui_SOURCE_DIR})

    add_library(reals_imgui_glfw STATIC
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
    target_include_directories(reals_imgui_glfw PUBLIC ${imgui_SOURCE_DIR}/backends)
    target_link_libraries(reals_imgui_glfw PUBLIC reals_imgui glfw)

    add_library(reals_ui STATIC
        ui/src/Theme.cpp
        ui/src/Widgets.cpp
        ui/src/AppShell.cpp
        ui/src/BrowserPane.cpp
        ui/src/BrowserPaneMenus.cpp)
    target_include_directories(reals_ui PUBLIC ${CMAKE_CURRENT_LIST_DIR}/ui/include)
    target_link_libraries(reals_ui PUBLIC reals_core reals_imgui)

    add_executable(reals_app app/main.cpp)
    target_link_libraries(reals_app PRIVATE reals_ui reals_imgui_glfw)
    if (WIN32)
        target_link_libraries(reals_app PRIVATE opengl32)
    elseif (APPLE)
        target_link_libraries(reals_app PRIVATE "-framework OpenGL")
    else()
        target_link_libraries(reals_app PRIVATE GL)
    endif()
    endif() # EXISTS app/main.cpp
endif()

# ---- REAPER extension (WebView2 UI) -------------------------------------------
if (REALS_BUILD_EXTENSION)
    add_subdirectory(extension)
endif()

# ---- Tests -------------------------------------------------------------------
enable_testing()

if (EXISTS ${CMAKE_CURRENT_LIST_DIR}/tests/test_soundtouch_processor.cpp)
    add_executable(test_soundtouch_processor tests/test_soundtouch_processor.cpp)
    target_include_directories(test_soundtouch_processor PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/core/include
        ${CMAKE_CURRENT_LIST_DIR}/tests)
    target_link_libraries(test_soundtouch_processor PRIVATE reals_core)
    add_test(NAME test_soundtouch_processor COMMAND test_soundtouch_processor)
endif()

if (EXISTS ${CMAKE_CURRENT_LIST_DIR}/tests/test_audio_engine.cpp)
    add_executable(test_audio_engine tests/test_audio_engine.cpp)
    target_include_directories(test_audio_engine PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/core/include
        ${CMAKE_CURRENT_LIST_DIR}/tests)
    target_link_libraries(test_audio_engine PRIVATE reals_core)
    add_test(NAME test_audio_engine COMMAND test_audio_engine)
endif()

if (EXISTS ${CMAKE_CURRENT_LIST_DIR}/tests/test_ai.cpp)
    add_executable(test_ai tests/test_ai.cpp)
    target_include_directories(test_ai PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/core/include
        ${CMAKE_CURRENT_LIST_DIR}/tests)
    target_link_libraries(test_ai PRIVATE reals_core)
    add_test(NAME test_ai COMMAND test_ai)
endif()

if (EXISTS ${CMAKE_CURRENT_LIST_DIR}/tests/test_db_scanner.cpp)
    add_executable(test_db_scanner tests/test_db_scanner.cpp)
    target_include_directories(test_db_scanner PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/core/include
        ${CMAKE_CURRENT_LIST_DIR}/tests)
    target_link_libraries(test_db_scanner PRIVATE reals_core)
    add_test(NAME test_db_scanner COMMAND test_db_scanner)
endif()

if (EXISTS ${CMAKE_CURRENT_LIST_DIR}/tests/CMakeLists.txt)
    add_subdirectory(tests)
endif()

