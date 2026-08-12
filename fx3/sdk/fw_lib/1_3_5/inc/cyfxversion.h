/*
 ## Cypress USB 3.0 Platform header file (cyfxversion.h)
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

#ifndef _INCLUDED_CYFXVERSION_H_
#define _INCLUDED_CYFXVERSION_H_

/** \file cyfxversion.h
    \brief Version information for the FX3 API library.

    <B>Description</B>\n
    The version information is composed of four values.

    <B>Note</B>\n
    The version information is compiled into the library and is provided here for reference.\n
    These values should not be changed and can only be used to conditionally enable code.
 */

/** \def CYFX_VERSION_MAJOR
    \brief Major number of the release version.
 */
#define CYFX_VERSION_MAJOR (1)

/** \def CYFX_VERSION_MINOR
    \brief Minor number of the release version.
 */
#define CYFX_VERSION_MINOR (3)

/** \def CYFX_VERSION_PATCH
    \brief Patch number for this release.
 */
#define CYFX_VERSION_PATCH (5)

/** \def CYFX_VERSION_BUILD
    \brief Build number.
 */
#define CYFX_VERSION_BUILD (62u)

#endif /* _INCLUDED_CYFXVERSION_H_ */

/*[]*/

