/*
 ## Cypress FX3 Core Library Header (cyu3externcend.h)
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


/** \file cyu3externcend.h
    \brief This file is included at the end of other include files.  It basically turns
    off the C++ specific code words that insure this code is seen as C code even by
    a C++ compiler.
 */

#ifdef __cplusplus
}
#endif
