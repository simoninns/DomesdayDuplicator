/*
 ## Cypress FX3 Core Library Header (cyu3types.h)
 ## ===========================
 ##
 ##  Copyright Cypress Semiconductor Corporation, 2010-2023,
 ##  All Rights Reserved
 ##  UNPUBLISHED, LICENSED SOFTWARE.
 ##
 ##  CONFIDENTIAL AND PROPRIETARY INFORMATION
 ##  WHICH IS THE PROPERTY OF CYPRESS.
 ##
 ##  Use of this file is governed
 ##  by the license agreement included in the file
 ##
 ##     <install>/license/license.txt
 ##
 ##  where <install> is the Cypress software
 ##  installation root directory path.
 ##
 ## ===========================
 */

#ifndef _INCLUDED_CYU3TYPES_H_
#define _INCLUDED_CYU3TYPES_H_

/** \file cyu3types.h
    \brief This file contains definitions for the basic data types used by the
    FX3 firmware library. If a compiler provided version is available, this can
    be used instead of the custom ones, by disabling the CYU3_DEFINE_BASIC_TYPES
    definition.
 */
#ifdef CYU3_DEFINE_BASIC_TYPES

/* Signed integer types. */
typedef signed   char      int8_t;      /** 8-bit signed integer. */
typedef signed   short     int16_t;     /** 16-bit signed integer. */
typedef signed   int       int32_t;     /** 32-bit signed integer. */
typedef signed   long long int64_t;     /** 64-bit signed integer. This is not used in the library, and should be
                                            used with caution in the user code. */

/* Unsigned integer types. */
typedef unsigned char      uint8_t;     /** 8-bit unsigned integer. */
typedef unsigned short     uint16_t;    /** 16-bit unsigned integer. */
typedef unsigned int       uint32_t;    /** 32-bit unsigned integer. */
typedef unsigned long long uint64_t;    /** 64-bit unsigned integer. This is not used in the library, and should be
                                            used with caution in the user code. */

/* MACROS for min and max values for various types. */

#define INT8_MIN        (-128)
#define INT8_MAX        (127)
#define UINT8_MAX       (255)

#define INT16_MIN       (-32768)
#define INT16_MAX       (32767)
#define UINT16_MAX      (65535)

#define INT32_MIN       (~0x7FFFFFFF)
#define INT32_MAX       (2147483647)
#define UINT32_MAX      (4294967295u)

#else

/* Use the standard (compiler provided) versions. */
#include <stdint.h>

#endif

/*
 * Unsigned volatile integer types for register access. These are not
 * expected to be provided by the compiler.
 */

typedef volatile unsigned char  uvint8_t;       /**< Volatile 8-bit unsigned value. This is not used in the library. */
typedef volatile unsigned short uvint16_t;      /**< Volatile 16-bit unsigned value. This is not used in the library. */
typedef volatile unsigned long  uvint32_t;      /**< Volatile 32-bit unsigned value. Used to represent FX3 device
                                                    registers. */

typedef int CyBool_t;                           /**< Boolean data type. */
#define CyTrue                  (1)             /**< Truth value. */
#define CyFalse                 (0)             /**< False value. */

/** \brief Return status from FX3 APIs.

    <B>Description</B>\n
    This is a custom data type used for return status from all FX3 APIs. The specific
    error codes that can be returned by these APIs are listed in cyu3error.h.

    **\see
    *\see CyU3PErrorCode_t
 */
typedef unsigned int CyU3PReturnStatus_t;

#endif /* _INCLUDED_CYU3TYPES_H_ */

/*[]*/

