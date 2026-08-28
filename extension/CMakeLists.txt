# REAPER extension shell (Windows — WebView2 UI)
set(REALS_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libs/reaper-sdk/sdk)
set(REALS_WEBVIEW2_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libs/webview2/build/native)

add_library(reals_shell_win STATIC
    ../shell/win/WebViewHost.cpp
    ../shell/win/OleDrag.cpp)
target_include_directories(reals_shell_win
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../shell/win ${REALS_WEBVIEW2_DIR}/include)
target_link_libraries(reals_shell_win PUBLIC reals_bridge)
target_link_libraries(reals_shell_win PUBLIC "${REALS_WEBVIEW2_DIR}/x64/WebView2LoaderStatic.lib" ole32 uuid shell32)
target_compile_definitions(reals_shell_win PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)

add_library(reaper_realslab SHARED src/reaper_plugin.cpp res/resource.rc)
target_include_directories(reaper_realslab PRIVATE ${REALS_SDK_DIR} res)
target_link_libraries(reaper_realslab PRIVATE reals_shell_win reals_bridge reals_core dwmapi)
target_compile_definitions(reaper_realslab PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_definitions(reaper_realslab PRIVATE
    REALS_UI_WEB_DIR_W=L\"${CMAKE_CURRENT_SOURCE_DIR}/../ui-web\")
if (MSVC)
    # reaper_plugin.h inline stubs trigger C4100/C4505 — not our code.
    target_compile_options(reaper_realslab PRIVATE /wd4100 /wd4505)
endif()

if (WIN32)
    add_custom_command(TARGET reaper_realslab POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$ENV{APPDATA}/REAPER/UserPlugins"
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -Command "\"Get-ChildItem '$ENV{APPDATA}/REAPER/UserPlugins/$<TARGET_FILE_NAME:reaper_realslab>*.old' -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue; if (Test-Path '$ENV{APPDATA}/REAPER/UserPlugins/$<TARGET_FILE_NAME:reaper_realslab>') { Move-Item -Force '$ENV{APPDATA}/REAPER/UserPlugins/$<TARGET_FILE_NAME:reaper_realslab>' ('$ENV{APPDATA}/REAPER/UserPlugins/$<TARGET_FILE_NAME:reaper_realslab>.' + (Get-Random) + '.old') -ErrorAction SilentlyContinue }; Copy-Item -Force '$<TARGET_FILE:reaper_realslab>' '$ENV{APPDATA}/REAPER/UserPlugins/$<TARGET_FILE_NAME:reaper_realslab>'\""
        COMMENT "Deploying reaper_realslab.dll to %APPDATA%/REAPER/UserPlugins"
    )
endif()
