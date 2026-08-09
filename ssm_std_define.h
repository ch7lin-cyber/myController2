
/**
 * @file ssm_std_define.h
 * @brief Main include file for the SSM FB dein.
 *
 */

#ifndef SSM_STD_FB_DEFINE_H_
#define SSM_STD_FB_DEFINE_H_

#include <stdbool.h>  /* NOLINT */
#include <stdint.h>   /* NOLINT */



#define SUCCESS 0x00000000
#define FAIL    0x00000001



// 在 PC 編譯時，確保函式可以被外部呼叫
#if defined(_WIN32) || defined(_WIN32_)
    #define MY_API __declspec(dllexport)
#else
    #define MY_API
#endif















#endif  // SSM_STD_FB_DEFINE_H_
