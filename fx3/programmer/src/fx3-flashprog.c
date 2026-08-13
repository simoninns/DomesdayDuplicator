/*
 * Domesday Duplicator - FX3 programmer
 * SPDX-FileCopyrightText: 2026 Simon Inns
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "fx3-flashprog.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

const char *fx3_flashprog_install_path(void)
{
#ifdef FLASHPROG_INSTALL_PATH
    return FLASHPROG_INSTALL_PATH;
#else
    return NULL;
#endif
}

char *fx3_find_flashprog_image_with_env(const char *env_path)
{
    const char *candidates[] = {
        /* An explicit override always wins */
        (env_path && env_path[0] != '\0') ? env_path : NULL,

        /* Where `make install` put it. This is what makes an installed binary work from
         * any working directory, rather than only from its own build tree. */
        fx3_flashprog_install_path(),

        /* Relative to the current directory, for running out of a build tree */
        "cyfxflashprog.img",
        "../cyfxflashprog.img",
        "../../../../../cyusb_linux/fx3_images/cyfxflashprog.img",
        "../../cyusb_linux/fx3_images/cyfxflashprog.img",
        "../fx3_images/cyfxflashprog.img",
        "../../fx3_images/cyfxflashprog.img",
    };
    struct stat st;

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        const char *path = candidates[i];
        if (!path) {
            continue;
        }
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            return strdup(path);
        }
    }

    return NULL;
}

char *fx3_find_flashprog_image(void)
{
    return fx3_find_flashprog_image_with_env(getenv("FX3_FLASH_PROG"));
}
