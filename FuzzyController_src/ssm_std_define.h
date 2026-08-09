/**
 * @file ssm_std_define.h
 * @brief Common cross-platform definitions for the Fuzzy/SSM FB library.
 */

#ifndef SSM_STD_FB_DEFINE_H_
#define SSM_STD_FB_DEFINE_H_

#include <stdbool.h>
#include <stdint.h>

#define SUCCESS 0x00000000
#define FAIL    0x00000001

/*
====================================================
 Cross-platform public API visibility
====================================================

Windows DLL build:
    define SSM_FB_BUILD_DLL

Windows DLL consumer:
    define SSM_FB_USE_DLL

Linux / GCC shared-library build:
    define SSM_FB_BUILD_SHARED

Static library, executable, MCU and other platforms:
    no additional define is required.
====================================================
*/

#if defined(_WIN32) || defined(__CYGWIN__)

    #if defined(SSM_FB_BUILD_DLL)
        #define MY_API __declspec(dllexport)
    #elif defined(SSM_FB_USE_DLL)
        #define MY_API __declspec(dllimport)
    #else
        #define MY_API
    #endif

#elif defined(__GNUC__) && defined(SSM_FB_BUILD_SHARED)

    #define MY_API __attribute__((visibility("default")))

#else

    #define MY_API

#endif

#endif /* SSM_STD_FB_DEFINE_H_ */
