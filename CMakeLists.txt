cmake_minimum_required(VERSION 3.20)

project(myController2
    VERSION 2.0.0
    LANGUAGES C CXX
)

include(CTest)

option(MYCONTROLLER_BUILD_SHARED "Build Fuzzy controller as a shared library" OFF)
option(MYCONTROLLER_BUILD_TESTS "Build C/C++ regression tests" ON)
option(MYCONTROLLER_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(MYCONTROLLER_BUILD_PC_CONTROL "Build ctypes bridge for Python PC control" ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(FUZZY_CONTROLLER_SOURCES
    FuzzyController_src/FB_FuzzyController.c
    FuzzyController_src/FB_FuzzyScaling.c
    FuzzyController_src/FB_FuzzyMembership.c
    FuzzyController_src/FB_FuzzyRule.c
    FuzzyController_src/FB_FuzzyOutputManager.c
    FuzzyController_src/FB_FuzzyHybridOutput.c
    FuzzyController_src/FB_FuzzyDefuzzifier.c
    FuzzyController_src/FB_FuzzyConfigManager.c
)

if(MYCONTROLLER_BUILD_SHARED)
    add_library(fuzzy_controller SHARED ${FUZZY_CONTROLLER_SOURCES})
else()
    add_library(fuzzy_controller STATIC ${FUZZY_CONTROLLER_SOURCES})
endif()

add_library(myController2::fuzzy_controller ALIAS fuzzy_controller)

target_include_directories(fuzzy_controller
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/FuzzyController_src>
)

# MY_API policy from ssm_std_define.h.
if(MYCONTROLLER_BUILD_SHARED)
    if(WIN32)
        target_compile_definitions(fuzzy_controller PRIVATE SSM_FB_BUILD_DLL)
        target_compile_definitions(fuzzy_controller INTERFACE SSM_FB_USE_DLL)
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_definitions(fuzzy_controller PRIVATE SSM_FB_BUILD_SHARED)
        set_target_properties(fuzzy_controller PROPERTIES
            C_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN YES
        )
    endif()
endif()

if(MSVC)
    target_compile_options(fuzzy_controller PRIVATE /W4)
    if(MYCONTROLLER_WARNINGS_AS_ERRORS)
        target_compile_options(fuzzy_controller PRIVATE /WX)
    endif()
else()
    target_compile_options(fuzzy_controller PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
    )
    if(MYCONTROLLER_WARNINGS_AS_ERRORS)
        target_compile_options(fuzzy_controller PRIVATE -Werror)
    endif()
endif()

# Small shared-library bridge for Python ctypes.  It compiles the controller
# sources directly so Python only needs one DLL/.so/.dylib at runtime.
if(MYCONTROLLER_BUILD_PC_CONTROL)
    add_library(fuzzy_pc_bridge SHARED
        tools/pc_control/FuzzyPcBridge.c
        ${FUZZY_CONTROLLER_SOURCES}
    )

    target_include_directories(fuzzy_pc_bridge
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/FuzzyController_src
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/pc_control
    )

    if(WIN32)
        target_compile_definitions(fuzzy_pc_bridge PRIVATE SSM_FB_BUILD_DLL)
        # MinGW normally prefixes DLL targets with "lib".  The Python tool
        # expects fuzzy_pc_bridge.dll on Windows, so normalize the filename
        # across MSVC and MinGW.
        set_target_properties(fuzzy_pc_bridge PROPERTIES PREFIX "")
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_definitions(fuzzy_pc_bridge PRIVATE SSM_FB_BUILD_SHARED)
        set_target_properties(fuzzy_pc_bridge PROPERTIES
            C_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN YES
        )
    endif()

    if(MSVC)
        target_compile_options(fuzzy_pc_bridge PRIVATE /W4)
    else()
        target_compile_options(fuzzy_pc_bridge PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

# Thermal identification plant is kept separate from the reusable controller library.
add_library(thermal_plant STATIC
    ControllPlant/myPlant_1.c
)

add_library(myController2::thermal_plant ALIAS thermal_plant)

target_include_directories(thermal_plant
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/ControllPlant
)

if(NOT MSVC)
    # expf/lroundf may require libm on Unix-like toolchains.
    target_link_libraries(thermal_plant PUBLIC m)
endif()

if(MYCONTROLLER_BUILD_TESTS AND BUILD_TESTING)
    add_subdirectory(tests/cmake)
endif()
