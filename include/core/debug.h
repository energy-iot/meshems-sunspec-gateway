/**
 * @file debug.h
 * @brief Debug logging macros gated by the ENABLE_DEBUG build flag.
 *
 * When ENABLE_DEBUG is not defined every macro expands to nothing, so debug
 * format strings are dropped from the binary rather than merely skipped.
 */

#pragma once

#ifndef DEBUG_PORT
// Serial2 is available as an alternative on the S3 UART pins; call
// DEBUG_PORT.setPins(18, 17) in setup() if that is used instead.
#define DEBUG_PORT Serial
#endif

#ifdef ENABLE_DEBUG
    #define DBUGS               DEBUG_PORT
    #define DEBUG_BEGIN(speed)  DEBUG_PORT.begin(speed)
    #define DBUGF(format, ...)  DEBUG_PORT.printf(format "\n", ##__VA_ARGS__)
    #define DBUG(...)           DEBUG_PORT.print(__VA_ARGS__)
    #define DBUGLN(...)         DEBUG_PORT.println(__VA_ARGS__)
#else
    #define DBUGS               DEBUG_PORT
    #define DEBUG_BEGIN(speed)
    #define DBUGF(...)
    #define DBUG(...)
    #define DBUGLN(...)
#endif
