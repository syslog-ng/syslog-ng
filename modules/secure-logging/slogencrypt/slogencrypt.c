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
 *
 */

#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <locale.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <openssl/crypto.h>
#include <unistd.h> //-- fsync + close for macOS

#include "messages.h"
#include "slog.h"
#include "utils_slog.h"
#include "compat/string.h"

//
// main logic: 0 usually indicates success, and non-zero values indicate an error
//
int main(int argc, char *argv[])
{
  gint retval = 0; //-- 0: SUCCESS, main logic
  guint64 bufSize = DEF_BUF_SIZE;
  gboolean is_verbose = FALSE;
  enum LogMode logmode = LOGMODE_ENCRYPTED;
  setlocale(LC_ALL, "");

  if (TRUE == is_verbose)
    {
      g_print("ENTER slogencrypt: argc: %d\n", argc);
      for (int i = 0; i < argc; i++)
        {
          g_print("   argv[%d]: %s\n", i, argv[i]);
        }
    }

  //-- Note in regard to plain mode: When plain mode, log entries migth be available as Base64 coded string - but without encryption!
  SLogOptions options[] =
  {
    { "key-file", 'k', "Current host key file", "FILE", NULL },
    { "mac-file", 'm', "Current MAC file", "FILE", NULL },
    { "logmode",  'l', "Log mode (direct|base64|enc) whether log is expected as is (direct) or plain but encoded as base64 or encrypted", "LOGMODE", NULL },
    { 0 }
  };

  GOptionEntry entries[] =
  {
    { options[0].longname, options[0].shortname, 0, G_OPTION_ARG_CALLBACK, &validFileNameArg, options[0].description, options[0].type },
    { options[1].longname, options[1].shortname, 0, G_OPTION_ARG_CALLBACK, &validFileNameArgCheckDirOnly, options[1].description, options[1].type },
    { options[2].longname, options[2].shortname, 0, G_OPTION_ARG_CALLBACK, &validLogModeArg, options[2].description, options[2].type },
    { 0 }
  };

  GError *error = NULL;
  GOptionContext *context =
    g_option_context_new("NEWKEY NEWMAC INPUTLOG OUTPUTLOG [COUNTER] - Encrypt log files using secure logging\n\n" \
                         "Example:\n" \
                         "  ./slogencrypt\n" \
                         "  --key-file ./current_host.key\n" \
                         "  --mac-file ./current_mac.dat\n" \
                         "  --logmode enc\n" \
                         "  ./new_host.key\n" \
                         "  ./new_mac.dat\n" \
                         "  ./input_log.txt\n" \
                         "  ./output_log.out\n"
                        );

  GOptionGroup *group = g_option_group_new("Basic options", "Basic log encryption options", "basic", &options, NULL);

  g_option_group_add_entries(group, entries);
  g_option_context_set_main_group(context, group);


  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_print("!g_opton_context_parse, argc: %d\n", argc);
      GString *errorMsg = g_string_new(error->message);
      (void) slog_usage(context, group, errorMsg);
      g_error_free(error);
      return 1; //-- ERROR
    }

  if (TRUE == is_verbose)
    {
      g_print("AFTER g_option_context_parse slogencrypt: argc: %d\n", argc);
      for (int i = 0; i < argc; i++)
        {
          g_print("   argv[%d]: %s\n", i, argv[i]);
        }
    }

  // Note: When all data is provided correctly, argc is 5 or 6 after parsing
  if ((argc < 5) || (argc > 6))
    {
      g_print("ERROR: Count of arguments is out of range!\n\n");
      (void) slog_usage(context, group, NULL);
      return 1; //-- ERROR
    }

  // Initialize internal messaging
  msg_init(TRUE);

  //-- When error then goto cleanup section -----
  char *line = NULL;
  int f_descriptor = -1;
  FILE *fp_outputFile = NULL;
  FILE *fp_inputFile = NULL;
  guchar key[KEY_LENGTH];
  guchar mac[CMAC_LENGTH];
  gsize nk = G_N_ELEMENTS(key);
  memset(key, 0, nk);
  gsize nm = G_N_ELEMENTS(mac);
  memset(mac, 0, nm);

  gint optidx = 0;

  GString *gstr_path_hostkey = g_string_new(NULL); //-- key-file
  GString *gstr_path_inputMAC = g_string_new(NULL); //-- mac-file
  GString *gstr_path_newhostKey = g_string_new(NULL); //-- NEWKEY
  GString *gstr_path_outputMAC = g_string_new(NULL); //-- NEWMAC
  GString *gstr_path_inputlog = g_string_new(NULL); //-- INPUTLOG
  GString *gstr_path_outputlog = g_string_new(NULL); //-- OUTPUTLOG

  //-- key-file (hostkey) ---
  if (NULL == options[optidx].arg)
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("Reason",
                            "Option --mac-file or -m does not provide a valid path to a MAC file!"));
      (void) slog_usage(context, group, NULL);
      context = NULL;
      retval = 1; //-- ERROR
      goto CLEANUP_SLOGENCRYPT;
    }
  {
    char *p_temp = g_strndup(options[optidx].arg, PATH_MAX - 1); //-- limit buffer
    g_free(options[optidx].arg);
    options[optidx++].arg = NULL; //-- inc
    char *p_canon = g_canonicalize_filename(p_temp, NULL); //-- normalize
    g_string_assign(gstr_path_hostkey, p_canon ? p_canon : "");
    g_free(p_temp);
    g_free(p_canon);
    if (gstr_path_hostkey->len == 0 ||
        !is_file_path_safe_and_valid(gstr_path_hostkey->str) ||
        !g_file_test(gstr_path_hostkey->str, G_FILE_TEST_IS_REGULAR))
      {
        msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Key-file validation failed"));
        (void) slog_usage(context, group, NULL);
        context = NULL;
        retval = 1;
        goto CLEANUP_SLOGENCRYPT;
      }
  }
  msg_info(SLOG_INFO_PREFIX, evt_tag_str("key-file", gstr_path_hostkey->str));


  //-- mac-file (inputMAC) ---
  if (NULL == options[optidx].arg)
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("Reason",
                            "Option --mac-file or -m does not provide a valid path to a MAC file!"));
      (void) slog_usage(context, group, NULL);
      context = NULL;
      retval = 1; //-- ERROR
      goto CLEANUP_SLOGENCRYPT;
    }
  {
    char *p_temp = g_strndup(options[optidx].arg, PATH_MAX - 1); //-- limit buffer
    g_free(options[optidx].arg);
    options[optidx++].arg = NULL; //-- inc
    char *p_canon = g_canonicalize_filename(p_temp, NULL); //-- normalize
    g_string_assign(gstr_path_inputMAC, p_canon ? p_canon : "");
    g_free(p_temp);
    g_free(p_canon);
    if (gstr_path_inputMAC->len == 0 ||
        !is_file_path_safe_and_valid(gstr_path_inputMAC->str)) //-- file might not exists yet
      {
        msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Check of mac-file failed"));
        (void) slog_usage(context, group, NULL);
        context = NULL;
        retval = 1; //-- ERROR
        goto CLEANUP_SLOGENCRYPT;
      }
  }
  msg_info(SLOG_INFO_PREFIX, evt_tag_str("mac-file", gstr_path_inputMAC->str));

  //-- logmode (direct|base64|enc) ---
  if (NULL == options[optidx].arg)
    {
      msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Old configuration without logmode: Use default enc"));
      logmode = LOGMODE_ENCRYPTED;
    }
  else
    {
      //-- variant --logmode (direct|base64|enc) provided
      char *str_logmode_arg = g_strndup(options[optidx].arg, PATH_MAX - 1); //-- limit buffer
      g_free(options[optidx].arg);
      options[optidx].arg = NULL;
      logmode = convert_str_logmode(str_logmode_arg);
      g_free(str_logmode_arg);
      str_logmode_arg = NULL;
      if (LOGMODE_PLAIN_DIRECT != logmode && LOGMODE_PLAIN_BASE64 != logmode
          && LOGMODE_ENCRYPTED != logmode)
        {
          msg_error(SLOG_ERROR_PREFIX,
                    evt_tag_str("Reason",
                                "Option --logmode or -l does not provide a valid logmode"));
          retval = 1; //-- ERROR
          goto CLEANUP_SLOGENCRYPT;
        }
    }
  GString *gstr_logmode = convert_logmode_str(logmode);
  if (gstr_logmode)
    {
      msg_info(SLOG_INFO_PREFIX, evt_tag_str("logmode", gstr_logmode->str));
      g_string_free(gstr_logmode, TRUE);
      gstr_logmode = NULL;
    }

  //-- Input and output file arguments -----

  //-- NEWKEY (newhostKey) ---
  optidx = 1;
  if (NULL == argv[optidx])
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Path to NEWKEY is missing!"));
      (void) slog_usage(context, group, NULL);
      context = NULL;
      retval = 1; //-- ERROR
      goto CLEANUP_SLOGENCRYPT;
    }
  {
    char *p_temp = g_strndup(argv[optidx++], PATH_MAX - 1); //-- limit buffer, inc
    char *p_canon = g_canonicalize_filename(p_temp, NULL); //-- normalize
    g_string_assign(gstr_path_newhostKey, p_canon ? p_canon : "");
    g_free(p_temp);
    g_free(p_canon);
    if (gstr_path_newhostKey->len == 0 ||
        !is_file_path_safe_and_valid(gstr_path_newhostKey->str)) //-- file might not exists yet
      {
        msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Check of NEWKEY failed"));
        (void) slog_usage(context, group, NULL);
        context = NULL;
        retval = 1; //-- ERROR
        goto CLEANUP_SLOGENCRYPT;
      }
  }
  msg_info(SLOG_INFO_PREFIX, evt_tag_str("NEWKEY", gstr_path_newhostKey->str));

  //-- NEWMAC (outputMAC) ---
  if (NULL == argv[optidx])
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Path to NEWMAC is missing"));
      (void) slog_usage(context, group, NULL);
      context = NULL;
      retval = 1; //-- ERROR
      goto CLEANUP_SLOGENCRYPT;
    }
  {
    char *p_temp = g_strndup(argv[optidx++], PATH_MAX - 1); //-- limit buffer, inc
    char *p_canon = g_canonicalize_filename(p_temp, NULL); //-- normalize
    g_string_assign(gstr_path_outputMAC, p_canon ? p_canon : "");
    g_free(p_temp);
    g_free(p_canon);
    if (gstr_path_outputMAC->len == 0 ||
        !is_file_path_safe_and_valid(gstr_path_outputMAC->str)) //-- file might not exists yet
      {
        msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Check of NEWMAC failed"));
        (void) slog_usage(context, group, NULL);
        context = NULL;
        retval = 1; //-- ERROR
        goto CLEANUP_SLOGENCRYPT;
      }
  }
  msg_info(SLOG_INFO_PREFIX, evt_tag_str("NEWMAC", gstr_path_outputMAC->str));

  //-- INPUTLOG ---
  if (NULL == argv[optidx])
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Path to INPUTLOG is missing"));
      (void) slog_usage(context, group, NULL);
      context = NULL;
      retval = 1; //-- ERROR
      goto CLEANUP_SLOGENCRYPT;
    }
  {
    char *p_temp = g_strndup(argv[optidx++], PATH_MAX - 1); //-- limit buffer, inc
    char *p_canon = g_canonicalize_filename(p_temp, NULL); //-- normalize
    g_string_assign(gstr_path_inputlog, p_canon ? p_canon : "");
    g_free(p_temp);
    g_free(p_canon);
    if (gstr_path_inputlog->len == 0 ||
        !is_file_path_safe_and_valid(gstr_path_inputlog->str) ||
        !g_file_test(gstr_path_inputlog->str, G_FILE_TEST_IS_REGULAR))
      {
        msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Check of INPUTLOG failed"));
        (void) slog_usage(context, group, NULL);
        context = NULL;
        retval = 1; //-- ERROR
        goto CLEANUP_SLOGENCRYPT;
      }
  }
  msg_info(SLOG_INFO_PREFIX, evt_tag_str("INPUTLOG", gstr_path_inputlog->str));

  //-- OUTPUTLOG ---
  if (NULL == argv[optidx])
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Path to OUTPUTLOG is missing"));
      (void) slog_usage(context, group, NULL);
      context = NULL;
      retval = 1; //-- ERROR
      goto CLEANUP_SLOGENCRYPT;
    }
  {
    char *p_temp = g_strndup(argv[optidx++], PATH_MAX - 1); //-- limit buffer, inc
    char *p_canon = g_canonicalize_filename(p_temp, NULL); //-- normalize
    g_string_assign(gstr_path_outputlog, p_canon ? p_canon : "");
    g_free(p_temp);
    g_free(p_canon);
    if (gstr_path_outputlog->len == 0 ||
        !is_file_path_safe_and_valid(gstr_path_outputlog->str)) //-- file might not exists yet
      {
        msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Check of OUTPUTLOG failed"));
        (void) slog_usage(context, group, NULL);
        context = NULL;
        retval = 1; //-- ERROR
        goto CLEANUP_SLOGENCRYPT;
      }
  }
  msg_info(SLOG_INFO_PREFIX, evt_tag_str("OUTPUTLOG", gstr_path_outputlog->str));

  // Read key and counter
  guint64 counter;
  gboolean ret = readKey(key, &counter, gstr_path_hostkey->str);
  if (!ret)
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("Reason", "Unable to read host key!"),
                evt_tag_str("file", gstr_path_hostkey->str));
      retval = 1; //-- ERROR
      goto CLEANUP_SLOGENCRYPT;
    }

  // Buffer size arguments if applicable
  if (argc == 6)
    {
      int result = sscanf(argv[optidx], "%"G_GUINT64_FORMAT, &bufSize);
      if ((result == EOF) || (bufSize <= MIN_BUF_SIZE) || (bufSize > MAX_BUF_SIZE))
        {
          msg_error(SLOG_ERROR_PREFIX,
                    evt_tag_str("Reason", "Invalid buffer size."),
                    evt_tag_printf("Size", "%" G_GUINT64_FORMAT, bufSize),
                    evt_tag_int("Minimum buffer size", MIN_BUF_SIZE),
                    evt_tag_int("Maximum buffer size", MAX_BUF_SIZE));
          retval = 1; //-- ERROR
          goto CLEANUP_SLOGENCRYPT;
        }
    }


  //
  //-- Done argument parsing and validation -----
  //

  // Open input file
  fp_inputFile = fopen(gstr_path_inputlog->str, "r");
  if (NULL == fp_inputFile)
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("Reason", "Unable to open input log file!"),
                evt_tag_str("file", gstr_path_inputlog->str));
      retval = -1; //-- ERROR
      goto CLEANUP_SLOGENCRYPT;
    }

  // Open output file
  // CI GitHub CodeQL / File created without restricting permissions:
  // outputFile = fopen(outputlogpath,  "w");
  f_descriptor = open(gstr_path_outputlog->str, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (f_descriptor != -1)
    {
      fp_outputFile = fdopen(f_descriptor, "w");
    }
  if (NULL == fp_outputFile)
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("Reason", "Unable to open output log file!"),
                evt_tag_str("file", gstr_path_outputlog->str));
      retval = -1; //-- ERROR
      goto CLEANUP_SLOGENCRYPT;
    }

  //-- initial MAC file ---
  if (!g_file_test(gstr_path_inputMAC->str, G_FILE_TEST_IS_REGULAR))
    {
      //-- This is normal the first time, slogencrypt is called.
      msg_info(SLOG_INFO_PREFIX,
               evt_tag_str("Reason", "MAC file was not found and is created now (initial MAC file)."),
               evt_tag_str("file", gstr_path_inputMAC->str));
      create_initial_mac0(key, mac);
      if (writeAggregatedMAC(gstr_path_inputMAC->str, mac) == FALSE)
        {
          //-- ERROR: File was not written!
          msg_error(SLOG_ERROR_PREFIX,
                    evt_tag_str("Reason", "writeAggregatedMAC was not successful!"),
                    evt_tag_str("file", gstr_path_inputMAC->str));
          retval = -1; //-- ERROR
          goto CLEANUP_SLOGENCRYPT;
        }
      char pathMac0[PATH_MAX]; //-- full path of MAC0 file mac0.dat
      if (get_path_mac0(gstr_path_inputMAC->str, pathMac0, PATH_MAX) == FALSE)
        {
          msg_error(SLOG_ERROR_PREFIX,
                    evt_tag_str("Reason", "unable to extract path for MAC0!"),
                    evt_tag_str("file", gstr_path_inputMAC->str));
          retval = -1; //-- ERROR
          goto CLEANUP_SLOGENCRYPT;
        }
      if (writeAggregatedMAC(pathMac0, mac) == FALSE)
        {
          msg_error(SLOG_ERROR_PREFIX,
                    evt_tag_str("Reason", "writeAggregatedMAC (MAC0) was not successful!"),
                    evt_tag_str("file", gstr_path_inputMAC->str));

          retval = -1; //-- ERROR, file not written
          goto CLEANUP_SLOGENCRYPT;
        }
      if (!g_file_test(gstr_path_inputMAC->str, G_FILE_TEST_IS_REGULAR))
        {
          msg_error(SLOG_ERROR_PREFIX,
                    evt_tag_str("Reason", "Initial MAC file was not written as expected!"),
                    evt_tag_str("file", gstr_path_inputMAC->str));
          retval = -1; //-- ERROR, File not found!
          goto CLEANUP_SLOGENCRYPT;
        }
    }

  // Read MAC (if possible)
  if (readAggregatedMAC(gstr_path_inputMAC->str, mac) == 0)
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("Reason", "Unable to open input MAC file!"),
                evt_tag_str("file", gstr_path_inputMAC->str));

      retval = -1; //-- ERROR, File not found!
      goto CLEANUP_SLOGENCRYPT;
    }


  //
  //-- Read and process input file
  //

  msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Importing data..."));

  // Parse data
  size_t readLen = 0;
  gboolean outcome = TRUE;
  while (getline(&line, &readLen, fp_inputFile) != -1)
    {
      guchar outputmacdata[CMAC_LENGTH];
      GString *result = g_string_new(NULL);
      GString *inputGString = g_string_new(line);

#if IS_LIMIT_LOGSTR

      //-- 2025-12-03
      //   In the classic secure logging (not the crash recovery variant)
      //   there shall be no limitation in regard to the length of a
      //   log string.
      //   No truncation might be a security risk and this might also cause
      //   a problem when buffers are created on the stack instead of the heap.

      //-- 2025-10-20, The code is designed to work with any length of log line.
      //   For security reason the length of the utf-8 string is limited.
      //   Later, Crash Recovery shall be integrated and there, the length
      //   is limited to 2048 bytes (MESSAGE_LEN, slog.h).
      truncate_utf8_gstring(inputGString, MESSAGE_LEN);

#endif /* IS_LIMIT_LOGSTR, see slog.h */

      // Remove trailing '\n' from string
      g_string_truncate(inputGString, (inputGString->len) - 1);
      gsize outputmacdata_capacity = G_N_ELEMENTS(outputmacdata);
      outcome = sLogEntry(counter,
                          inputGString,
                          key,
                          mac,
                          result,
                          outputmacdata,
                          outputmacdata_capacity,
                          logmode);

      if (!outcome)
        {
          msg_warning(SLOG_WARNING_PREFIX,
                      evt_tag_str("Reason", "Unable to encrypt log entry!"),
                      evt_tag_str("line", inputGString->str));
          g_string_free(result, TRUE);
          g_string_free(inputGString, TRUE);
          retval = -1; //-- ERROR
          goto CLEANUP_SLOGENCRYPT;
        }

      int ret_fprintf = fprintf(fp_outputFile, "%s\n", result->str);
      if (ret_fprintf < 0)
        {
          GString *gstr_error_msg = g_string_new(NULL);
          g_string_printf(gstr_error_msg, "fprintf failed! Error: %s (code %d)\n", strerror(errno), errno);
          msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", gstr_error_msg->str));
          g_string_free(gstr_error_msg, TRUE);
          g_string_free(result, TRUE);
          g_string_free(inputGString, TRUE);
          retval = -1; //-- ERROR, failed to write file
          goto CLEANUP_SLOGENCRYPT;
        }

      // Update keys, MAC, etc
      memcpy(mac, outputmacdata, CMAC_LENGTH);
      outcome = evolveKey(key);
      if (!outcome)
        {
          msg_warning(SLOG_WARNING_PREFIX, evt_tag_str("Reason", "Unable to evolve key!"));
          g_string_free(result, TRUE);
          g_string_free(inputGString, TRUE);
          retval = -1; //-- ERROR, failed to evolve key!
          goto CLEANUP_SLOGENCRYPT;
        }

      g_string_free(result, TRUE);
      g_string_free(inputGString, TRUE);
      if (NULL != line)
        {
          free(line);
          line = NULL;
        }
      counter++;
      readLen = 0;

    } //-- while (getline


  // Write whole log MAC
  if (!writeAggregatedMAC(gstr_path_outputMAC->str, mac))
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Problem with output MAC file!"));
      retval = -1; //-- ERROR
      goto CLEANUP_SLOGENCRYPT;
    }

  // Write key
  ret = writeKey(key, counter, gstr_path_newhostKey->str);
  if (!ret)
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("Reason", "Unable to write new host key"),
                evt_tag_str("file", gstr_path_outputlog->str));
      retval = -1; //-- ERROR
      goto CLEANUP_SLOGENCRYPT;
    }


CLEANUP_SLOGENCRYPT:

  //-- erasure of sensitive data
  OPENSSL_cleanse(key, sizeof key);
  OPENSSL_cleanse(mac, sizeof mac);

  if (NULL != fp_inputFile)
    {
      fclose(fp_inputFile);
      fp_inputFile = NULL;
    }

  if (NULL != fp_outputFile)
    {
      fflush(fp_outputFile);
      int fd = fileno(fp_outputFile);
      if (-1 != fd)
        {
          fsync(fd);
        }
      fclose(fp_outputFile);
      fp_outputFile = NULL;
    }
  else if (f_descriptor >= 0)
    {
      //-- fd was opened but not wrapped in a stream
      close(f_descriptor);
      f_descriptor = -1;
    }

  if (NULL != context)
    {
      g_option_context_free(context);
      context = NULL;
    }
  if (NULL != line)
    {
      free(line);
      line = NULL;
    }

  g_string_free(gstr_path_hostkey, TRUE);
  gstr_path_hostkey = NULL;
  g_string_free(gstr_path_inputMAC, TRUE);
  gstr_path_inputMAC = NULL;
  g_string_free(gstr_path_newhostKey, TRUE);
  gstr_path_newhostKey = NULL;
  g_string_free(gstr_path_outputMAC, TRUE);
  gstr_path_outputMAC = NULL;
  g_string_free(gstr_path_inputlog, TRUE);
  gstr_path_inputlog = NULL;
  g_string_free(gstr_path_outputlog, TRUE);
  gstr_path_outputlog = NULL;

  if (0 == retval)
    {
      msg_info("slogencrypt: All data successfully imported.");
    }
  else
    {
      msg_info("slogencrypt: Error occurred.");
    }

  //-- Release messaging resources
  msg_deinit();

  return retval; //-- 0 when SUCCES
}
