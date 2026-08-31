cmake_minimum_required(VERSION 3.24)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/suites/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/unit/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/benchmarks/*.cpp)

add_executable(reals_tests ${TEST_SOURCES})

target_include_directories(reals_tests
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/framework
        ${CMAKE_SOURCE_DIR}/core/include
        ${CMAKE_SOURCE_DIR}/bridge/include
        ${miniaudio_SOURCE_DIR})

target_link_libraries(reals_tests
    PRIVATE
        reals_bridge
        reals_core
        nlohmann_json::nlohmann_json)

if (MSVC)
    target_compile_definitions(reals_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
    if (WIN32)
        target_link_libraries(reals_tests PRIVATE winhttp)
    endif()
endif()

enable_testing()
add_test(NAME reals_e2e_tests COMMAND reals_tests)
