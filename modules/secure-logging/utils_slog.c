/*
 * Copyright (c) 2019-2026 Airbus Commercial Aircraft
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
 */

#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <locale.h>
#include <ctype.h>
#include <unistd.h>

#include "slog.h" // SLogOptions
#include "utils_slog.h"

/*
 * Callback function to check whether a command line argument represents a valid log mode
 *
 * Return:
 * TRUE when is known log mode (valid string)
 * FALSE when unknown or no log mode provided (Caller then will use LOGMMODE_ENCRYPTED)
 */
gboolean validLogModeArg(const gchar *option_name, const gchar *value, gpointer data, GError **error)
{
  gboolean isValid = FALSE;
  if (NULL == option_name)
    {
      return isValid;
    }
  if (NULL == value)
    {
      return isValid;
    }
  if (NULL == data)
    {
      return isValid;
    }

  GString *currentOption = g_string_new(option_name);
  GString *currentValue = g_string_new(value); //-- direct | base64 |  enc
  GString *longOption = g_string_new(LONG_OPT_INDICATOR);
  GString *shortOption = g_string_new(SHORT_OPT_INDICATOR);

  SLogOptions *opts = (SLogOptions *)data;

  for (SLogOptions *option = opts; option != NULL && option->longname != NULL; option++)
    {
      g_string_append(longOption, option->longname);
      g_string_append_c(shortOption, option->shortname);

      if (g_string_equal(currentOption, longOption) || g_string_equal(currentOption, shortOption))
        {
          //-- LOGMODE_PLAIN_DIRECT
          if (g_ascii_strncasecmp(value, "direct", 6) == 0)
            {
              option->arg = currentValue->str;
              isValid = TRUE;
              break;
            }

          //-- LOGMODE_PLAIN_BASE64
          else if (g_ascii_strncasecmp(value, "base64", 6) == 0)
            {
              option->arg = currentValue->str;
              isValid = TRUE;
              break;
            }

          //-- LOGMODE_ENCRYPTED
          else if (g_ascii_strncasecmp(value, "enc", 3) == 0)
            {
              option->arg = currentValue->str;
              isValid = TRUE;
              break;
            }
        }

      // Reset for next option argument to check
      g_string_assign(longOption, LONG_OPT_INDICATOR);
      g_string_assign(shortOption, SHORT_OPT_INDICATOR);
    }

  if (!isValid)
    {
      if (NULL != error)
        {
          *error = g_error_new(G_FILE_ERROR, G_OPTION_ERROR_FAILED,
                               "Invalid argument given. One of (direct|base64|enc) expected!");
        }
      g_string_free(currentValue, TRUE);
    }
  else
    {
      char *dummy = g_string_free(currentValue, FALSE); //-- caller has to free data of GString
      if (NULL == dummy)
        {
          g_warning("Invalid string buffer, not expected");
        }
    }
  g_string_free(currentOption, TRUE);
  g_string_free(longOption, TRUE);
  g_string_free(shortOption, TRUE);

  return isValid;
}


/* Convert string argument value into enum LogMode.
 *
 * enum LogMode
 * {
 *   LOGMODE_PLAIN_DIRECT, //-- do NOT encrypt and do NOT Base64 encode log message
 *   LOGMODE_PLAIN_BASE64, //-- do NOT encrypt and do Base64 encode log message
 *   LOGMODE_ENCRYPTED,    //-- do encrypt and do Base64 encode log message
 * };
 *
 * retrun enum LogMode value
 */

enum LogMode convert_str_logmode(const gchar *value)
{
  enum LogMode retval = LOGMODE_ENCRYPTED;
  if (NULL == value)
    {
      g_print("convert_str_logmode, NULL pointer");
    }
  else if (g_ascii_strncasecmp(value, "direct", 6) == 0)
    {
      retval = LOGMODE_PLAIN_DIRECT;
    }
  else if (g_ascii_strncasecmp(value, "base64", 6) == 0)
    {
      retval = LOGMODE_PLAIN_BASE64;
    }
  g_print("convert_str_logmode, value: %s, logmode: %d\n", (NULL == value) ? "NULL" : value, retval);
  return retval;
}


/* Convert enum Logmode into a string representing syslog-ng.conf logmode.
 *
 * retrun GString* with logmode like in syslog-ng.conf. To be freed by caller.
 */

GString *convert_logmode_str(enum LogMode val)
{
  const gchar *str_val;
  switch (val)
    {
    case LOGMODE_PLAIN_DIRECT:
      str_val = "direct";
      break;
    case LOGMODE_PLAIN_BASE64:
      str_val = "base64";
      break;
    case LOGMODE_ENCRYPTED:
      str_val = "enc";
      break;
    case LOGMODE_INVALID:
    default:
      str_val = "invalid";
      break;
    }
  //-- g_string_new allocates memory on the heap.
  //   The caller must free this using g_string_free(gstr, TRUE).
  return g_string_new(str_val);
}

//----------------------------------------------------------------------
// truncate_uft8_gstring
//
// Ensures that when a log string must be truncated, no invalid
// UTF-8 characters is created.
// Also ensured: When truncation takes place, a '\n' character is added
// at the new end of the string.
//
// in/out gslog: GString to be truncated in a valid way
// in max_octet_len: Maximal allowed count of octets in gslog
// return -

void truncate_utf8_gstring(GString *gslog, gsize max_octet_len)
{
  if (NULL == gslog)
    {
      return;
    }
  if (gslog->len <= max_octet_len)
    {
      return;
    }
  if (0U == max_octet_len)
    {
      g_string_truncate(gslog, 0);
      return;
    }
  gsize max_text_len = max_octet_len - 1U; //-- reserve one byte for '\n'
  gsize new_len = max_text_len;
  //--Walk backwards to find a valid UTF-8 character / Symbol (Emoij) boundary.
  while ((new_len > 0U) && ((gslog->str[new_len] & 0xC0) == 0x80))
    {
      new_len--;
    }
  g_string_truncate(gslog, new_len);
  g_string_append_c(gslog, '\n');
}



//----------------------------------------------------------------------
// is_likely_base64
//
// Heuristic check if given zero terminated string could be a base64 coded
//
// in str: string to be checked
//
// return gboolean whether could be base64

gboolean is_likely_base64(const gchar *str)
{
  if ((str == NULL) || (*str == '\0'))
    {
      return FALSE;
    }

  //-- Check length (must be multiple of 4 if padded)
  gsize len = strlen(str);
  if ((len % 4U) != 0U)
    {
      return FALSE;
    }

  //-- Use Regex to check for valid Base64 characters
  //   This pattern ensures only A-Z, a-z, 0-9, +, / are used,
  //   and allows up to two '=' at the end.
  GRegex *regex = g_regex_new("^[A-Za-z0-9+/]+={0,2}$", 0, 0, NULL);
  gboolean match = g_regex_match(regex, str, 0, NULL);
  g_regex_unref(regex);

  return match;
}


//----------------------------------------------------------------------
// get_base64_length
//
// Helper to get the length in bytes of a Base64 coded string
//
// in uncoded_count: length of uncoded string
//
// return length of Base64 encoded string

gsize get_base64_length( gsize uncoded_count )
{
  // Formula: 4 * ceil(n / 3)
  // In integer arithmetic: ((n + 2) / 3) * 4
  return ((uncoded_count + 2U) / 3U) * 4U;
}

//----------------------------------------------------------------------
// get_decoded_base64_length
//
// Helper to get the length in bytes of a uncoded Base64 string
//
// in base64 encoded string
//
// return min length of a buffer to hold the uncoded string

gsize get_decoded_base64_length(const char *encoded_str)
{
  gsize len = strlen(encoded_str);
  gsize padding = 0U;
  // Check for padding characters at the end
  if ((len > 0U) && (encoded_str[len - 1U] == '=')) padding++;
  if ((len > 1U) && (encoded_str[len - 2U] == '=')) padding++;
  return (((len * 3U) / 4U) - padding);
}

//----------------------------------------------------------------------
// dbg_hexdump
//
// Prints a classic hexdump of a memory region.
//
// This function displays the data in a traditional 16-byte-per-line format.
// Each line shows the offset, the hexadecimal values of the bytes, and their
// printable ASCII representation.
// Non-printable characters are replaced with a dot (.).
//
// in title: A title to print before the hexdump for context.
// in data:  A pointer to the data buffer to be dumped.
// in len:   The length of the data buffer in bytes.
//
// return -

void dbg_hexdump(const char *title, const unsigned char *data, unsigned int len)
{
  const guint BYTES_PER_LINE = 16U;
  const guint MAX_DBG_LEN = 3072U;

  // Print a title for the hexdump
  // g_print("\n--- Hexdump: %s (%d bytes) ---\n", title, len);

  g_print("%s (%d bytes):\n", title, len);
  if ((data == NULL) || (len == 0U))
    {
      g_print("--- dbg_hexdump: empty or NULL pointer\n\n");
      return;
    }

  guint limlen = (guint) len;
  if (limlen > MAX_DBG_LEN)
    {
      limlen = MAX_DBG_LEN;
      g_print("--- len: %d limited for debug to limlen: %d\n\n", len, limlen);
    }

  //-- Iterate over the data in chunks of `BYTES_PER_LINE` ---
  for (guint i = 0U; i < limlen; i += BYTES_PER_LINE)
    {
      // Print the offset for the current line
      g_print("%08d: ", i);

      //-- Print the hexadecimal representation of bytes for the current line ---
      for (guint j = 0U; j < BYTES_PER_LINE; ++j)
        {
          // Add extra space in the middle for readability (after 8 bytes)
          if (j == 8U)
            {
              g_print(" ");
            }

          // Check if we are still within the bounds of the data
          if ((i + j) < limlen)
            {
              // %02x prints a hex value, padded with a zero to 2 characters
              g_print("%02x ", data[i + j]);
            }
          else
            {
              // Print spaces to align the final partial line
              g_print("   ");
            }
        }

      g_print(" |"); // Separator before the character representation

      //-- Print the printable character representation ---
      for (guint j = 0U; j < BYTES_PER_LINE; ++j)
        {
          if ((i + j) < limlen)
            {
              guint8 byte = data[i + j];
              // isprint() checks for any printable character including space.
              // This provides a good-enough view for debugging UTF-8 data,
              // as it will show the ASCII parts clearly.
              if (isprint(byte))
                {
                  g_print("%c", byte);
                }
              else
                {
                  g_print(".");
                }
            }
        }
      g_print("|\n"); // Closing separator and newline
    }
  //  g_print("--- End of hexdump ---\n\n");
}


//----------------------------------------------------------------------
// is_file_path_safe_and_valid
//
// Checks wether a file path argument is safe and valid.
// It does NOT check if the file exists and it SHALL NOT check.
// Only the existens of extracted directory is verified because the file
// might be created.
//
// in input_path: zero terminated path string
// return TRUE when valid and usable else FALSE

gboolean is_file_path_safe_and_valid(const gchar *input_path)
{
  gboolean retval = FALSE;
  gchar *safe_path = NULL;
  gchar *dir_name = NULL;
  gchar *base_name = NULL;

  if ((input_path == NULL) || (*input_path == '\0'))
    {
      return FALSE;
    }
  safe_path = g_strndup(input_path, PATH_MAX - 1U);
  dir_name = g_path_get_dirname(safe_path);
  base_name = g_path_get_basename(safe_path);

  if (!g_file_test(dir_name, G_FILE_TEST_IS_DIR))
    {
      g_warning("Parent directory does not exist or is inaccessible: %s", dir_name);
      goto CLEANUP_IFPSAV;
    }

  if ((g_strcmp0(base_name, ".") == 0) || (g_strcmp0(base_name, "..") == 0))
    {
      g_warning("Invalid filename: %s", base_name);
      goto CLEANUP_IFPSAV;
    }

  if (access(dir_name, (W_OK | X_OK)) != 0)
    {
      g_warning("No write permissions in directory: %s", dir_name);
      goto CLEANUP_IFPSAV;
    }

  retval = TRUE;

CLEANUP_IFPSAV:
  g_free(safe_path);
  g_free(dir_name);
  g_free(base_name);
  return retval;
}

