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

#ifndef _SLOG_SECT_H_INCLUDED_
#define _SLOG_SECT_H_INCLUDED_

#include<setjmp.h>

#include <glib.h>

#define SLOG_INFO_PREFIX "[SLOG] INFO"
#define SLOG_ERROR_PREFIX "[SLOG] ERROR"
#define SLOG_FILE_DOMAIN 100
#define SLOG_FILE_READY 1000
#define SLOG_FILE_OPEN 1001
#define SLOG_FILE_CLOSED 1002
#define SLOG_FILE_GENERAL_ERROR 2000
#define SLOG_FILE_INVALID_MODE 2001
#define SLOG_FILE_EOF 2002
#define SLOG_FILE_RESOURCE_UNAVAILABLE 2003
#define SLOG_FILE_INCOMPLETE_READ 2004
#define SLOG_FILE_INCOMPLETE_WRITE 2005
#define SLOG_FILE_SHUTDOWN_ERROR 2006
#define SLOG_MEM_ALLOC_ERROR 3000
#define SLOG_OPENSSL_LIBRARY_ERROR 4000

#define SLOG_SECT_START   \
  for(slog_sect_t sect = SLogSectInit(); SLogSectCondition(&sect); ) { \
    if(setjmp(sect.env) == 0) {

#define SLOG_SECT_END(OBJ) \
      } else { \
      SLogSectHandleError(&sect, OBJ); } }

enum SLogSectState
{
    SLOG_SECT_INIT,      // Initial state
    SLOG_SECT_NORMAL,    // Normal operation
    SLOG_SECT_FORWARD,   // Forwarding
    SLOG_SECT_HANDLE,    // Handle error
};

// Error handler
typedef void (*slog_error_handler_t)(const char *file, int line, int code, void *object);

typedef enum SLogSectState slog_sect_state_t;

// Forward declaration for linked list
typedef struct SLogSect slog_sect_t;

// Error information
typedef struct SLogErrorInfo
{
    const char *file;
    int line;
    int code;
    slog_error_handler_t handler;
} slog_error_info_t;

// Linked list for error section
struct SLogSect
{
    jmp_buf env;
    slog_sect_t *parent;
    slog_sect_state_t state;
};

slog_sect_t SLogSectInit(void);
gboolean SLogSectCondition(slog_sect_t *sect);
void SLogSectForward(char const *file, int line, int code, slog_error_handler_t handler);
void SLogSectHandleError(slog_sect_t *sect, void *object);

#endif
