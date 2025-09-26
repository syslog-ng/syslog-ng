/*
 * Copyright (c) 2023 One Identity LLC.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include <syslog-ng-config.h>

#if SYSLOG_NG_HAVE_ZLIB

#include <criterion/criterion.h>
#include <criterion/redirect.h>
#include <unistd.h>
#include "compression.h"

#if SYSLOG_NG_HTTP_COMPRESSION_ENABLED

char *test_message =
  "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";

void
get_decompress_command(GString *command, GString *compression_result, enum CurlCompressionTypes compressionType)
{
  /**
   * NOTE: base64 encoding is necessary due to some bits being interpreted as control characters by the shell.
   */
  switch (compressionType)
    {
    case CURL_COMPRESSION_UNKNOWN:
      // Cannot test unknown compression. If necessary, maybe try 7zip???
      g_assert_not_reached();
      break;
    case CURL_COMPRESSION_UNCOMPRESSED:
      g_string_printf(command, "echo %s", compression_result->str);
      break;
    case CURL_COMPRESSION_GZIP:
      g_string_printf(command, "echo %s | base64 -d | gzip -dc",
                      g_base64_encode((guchar *)compression_result->str, compression_result->len));
      break;
    case CURL_COMPRESSION_DEFLATE:
      g_string_printf(command, "echo %s | base64 -d | zlib-flate -uncompress",
                      g_base64_encode((guchar *)compression_result->str, compression_result->len));
      break;
    default:
      g_assert_not_reached();
    }
}

static inline void
test_compression_results(GString *compression_result, enum CurlCompressionTypes compressionType)
{
  // The compression did indeed compress
  cr_assert_gt(strlen(test_message), compression_result->len);

  GString *command = g_string_new("");
  get_decompress_command(command, compression_result, compressionType);
  FILE *tempfile = popen(command->str, "r");
  sleep(10);

  char buffer[256];
  GString *uncompressed = g_string_new("");
  while (fgets(buffer, sizeof(buffer), tempfile) != NULL)
    g_string_append(uncompressed, buffer);
  cr_assert_str_eq(uncompressed->str, test_message);

  pclose(tempfile);
  g_string_free(command, TRUE);
}

Compressor *compressor;
GString *input, *result;

static void
setup(void)
{
  cr_redirect_stdout();
  cr_redirect_stderr();
  input = g_string_new(test_message);
}

static void
teardown(void)
{
  g_string_free(input, 0);
}

TestSuite(compression, .init = setup, .fini = teardown);

Test(compression, compressor_gzip_compression)
{
  compressor = gzip_compressor_new();
  cr_assert_not_null(compressor);
  result = g_string_new("");
  cr_assert(compressor_compress(compressor, result, input));
  test_compression_results(result, CURL_COMPRESSION_GZIP);
  compressor_free(compressor);
  g_string_free(result, TRUE);
}

// FIXME: zlib-flate is missing on 32 bit test image, hence no deflate-test
#if !defined(i386) && !defined(__i386__) && !defined(__i386) && !defined(_M_IX86)
Test(compression, compressor_deflate_compression)
{
  compressor = deflate_compressor_new();
  cr_assert_not_null(compressor);
  result = g_string_new("");
  cr_assert(compressor_compress(compressor, result, input));
  test_compression_results(result, CURL_COMPRESSION_DEFLATE);
  compressor_free(compressor);
  g_string_free(result, TRUE);
}
#endif

#endif

#endif
