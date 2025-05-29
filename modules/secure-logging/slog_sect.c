/*
 * Copyright (c) 2025 Airbus Commercial Aircraft
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include <stdio.h>
#include <setjmp.h>

#include "slog_sect.h"

// Section list head
static slog_sect_t *SLogSectHead;
static slog_error_info_t ErrorInfo;

slog_sect_t SLogSectInit(void)
{
    static slog_sect_t const sect;
    return sect;
}

gboolean SLogSectCondition(slog_sect_t *sect)
{
    switch(sect->state)
    {
    case SLOG_SECT_INIT:
        sect->parent = SLogSectHead;
        SLogSectHead = sect;
        sect->state = SLOG_SECT_NORMAL;
        return TRUE;
    case SLOG_SECT_FORWARD:
    case SLOG_SECT_NORMAL:
    case SLOG_SECT_HANDLE:
        SLogSectHead = sect->parent;
        return FALSE;
    default:
        break;
    } // switch
    return FALSE;
}

_Noreturn
void SLogSectForward(char const *file, int line, int code, slog_error_handler_t handler)
{
    ErrorInfo = (slog_error_info_t)
    {
        .file = file,
        .line = line,
        .code = code,
        .handler = handler
    };

    // Abort on unhanded errors
    if(SLogSectHead == NULL)
    {
        fprintf(stderr, "%s Unhandled error in file %s at line %d. Aborting.\n",
                SLOG_ERROR_PREFIX,
                file,
                line
               );
        abort();
    }
    SLogSectHead->state = SLOG_SECT_FORWARD;
    longjmp(SLogSectHead->env, 1);
}

void SLogSectHandleError(slog_sect_t *sect, void *object)
{
    SLogSectHead->state = SLOG_SECT_HANDLE;

    // Call actual error handler
    ErrorInfo.handler(ErrorInfo.file, ErrorInfo.line, ErrorInfo.code, object);
}
