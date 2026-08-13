/*
 * Locating the Cypress secondary loader, cyfxflashprog.img.
 *
 * Permanent (EEPROM / SPI flash) programming works by first pushing this Cypress-supplied
 * loader into FX3 RAM; it then performs the actual flash write. RAM-only downloads do not
 * need it.
 *
 * Split out of fx3-programmer.c so the search order can be unit tested — it is pure path
 * resolution, and getting it wrong is the difference between a working install and a tool
 * that only functions from one specific directory (D13).
 *
 * Domesday Duplicator - FX3 programmer
 * SPDX-FileCopyrightText: 2026 Simon Inns
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef FX3_FLASHPROG_H
#define FX3_FLASHPROG_H

/*
 * Return the path to cyfxflashprog.img, or NULL if no candidate exists.
 *
 * The caller owns the returned string and must free() it.
 *
 * Search order:
 *   1. $FX3_FLASH_PROG, if set and non-empty
 *   2. FLASHPROG_INSTALL_PATH, compiled in by CMake, if defined
 *   3. paths relative to the current working directory, for running from a build tree
 */
char *fx3_find_flashprog_image(void);

/*
 * As above, but with the environment variable supplied explicitly rather than read from
 * the process environment. Exists so tests can exercise the search order without mutating
 * global state. Pass NULL to skip the environment candidate.
 */
char *fx3_find_flashprog_image_with_env(const char *env_path);

/*
 * The compiled-in install path, or NULL if the build did not define one.
 * Exposed so the diagnostic on failure can name it, and so tests can assert it is set.
 */
const char *fx3_flashprog_install_path(void);

#endif /* FX3_FLASHPROG_H */
