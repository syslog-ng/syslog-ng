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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <glib.h>
#include <locale.h>
#include <openssl/crypto.h>

#include "messages.h"
#include "slog.h"
#include "utils_slog.h"

static gboolean is_verbose = FALSE;

// Return TRUE on success, FALSE on error
gboolean normalMode(char *path_hostkey, char *path_MACfile, char *path_inputlog, char *path_outputlog, guint32 bufsize,
                    enum LogMode logmode)
{
  guchar key[KEY_LENGTH];
  guint64 counter;

  msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Reading key file"), evt_tag_str("name", path_hostkey));
  gboolean success = readKey(key, &counter, path_hostkey);
  if (!success)
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to read host key"), evt_tag_str("file", path_hostkey));
      return FALSE; //-- ERROR
    }

  if (counter != 0UL)
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason",
                                               "Initial key k0 is required for verification and decryption but the supplied key read has a counter > 0."),
                evt_tag_printf("Counter", "%" G_GUINT64_FORMAT, counter));
      OPENSSL_cleanse(key, sizeof key);
      return FALSE; //-- ERROR
    }

  msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Reading MAC file"), evt_tag_str("name", path_MACfile));
  if ( ! g_file_test(path_MACfile, G_FILE_TEST_IS_REGULAR))
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to read MAC"), evt_tag_str("file", path_MACfile));
      OPENSSL_cleanse(key, sizeof key);
      return FALSE; //-- ERROR
    }

  guchar MAC[CMAC_LENGTH]; //-- aggregated MAC
  if (!readAggregatedMAC(path_MACfile, MAC))
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to read MAC"), evt_tag_str("file", path_MACfile));
      OPENSSL_cleanse(key, sizeof key);
      return FALSE; //-- ERROR
    }

  //-- initial MAC0 ---
  char pathMac0[PATH_MAX]; //-- full path of MAC0 file mac0.dat
  guchar MAC0[CMAC_LENGTH]; //-- initial MAC
  memset(MAC0, 0, CMAC_LENGTH);
  if (TRUE == get_path_mac0(path_MACfile, pathMac0, PATH_MAX))
    {
      if (!readAggregatedMAC(pathMac0, MAC0))
        {
          msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to read MAC0"), evt_tag_str("file", pathMac0));
          return FALSE; //-- ERROR
        }
      else
        {
          msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Read MAC0"), evt_tag_str("file", pathMac0));
        }
    }
  else
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Invalid pathMac0")); //-- fileVerify will fail later
      OPENSSL_cleanse(MAC0, sizeof MAC0);
      OPENSSL_cleanse(MAC, sizeof MAC);
      OPENSSL_cleanse(key, sizeof key);
      return FALSE; //-- ERROR
    }


  FILE *fp_input = fopen(path_inputlog, "r");
  if (fp_input == NULL)
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to open inputlog"), evt_tag_str("file", path_inputlog));
      OPENSSL_cleanse(MAC0, sizeof MAC0);
      OPENSSL_cleanse(MAC, sizeof MAC);
      OPENSSL_cleanse(key, sizeof key);
      return FALSE; //-- ERROR
    }

  // Scan through file ones
  guint64 entries = 0;
  int c;
  while ((c = fgetc(fp_input)) != EOF)
    {
      if (c == '\n')
        {
          entries++;
        }
    }

  fclose(fp_input);
  fp_input = NULL;

  msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Number of lines in file"), evt_tag_printf("number",
           "%" G_GUINT64_FORMAT, entries));
  msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Restoring and verifying log entries"), evt_tag_long("buffer size",
           bufsize));
  gboolean result = fileVerify(key,
                               path_inputlog,
                               path_outputlog,
                               MAC,
                               entries,
                               bufsize,
                               MAC0,
                               logmode);

  OPENSSL_cleanse(key, sizeof key);
  OPENSSL_cleanse(MAC, sizeof MAC);
  OPENSSL_cleanse(MAC0, sizeof MAC0);
  if (!result)
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason",
                                               "There is a problem with log verification. Please check log manually"));
      return FALSE; //-- ERROR

    }
  return TRUE; //-- SUCCESS
}


// Return TRUE on success, FALSE on error
gboolean iterativeMode(char *path_prevKey, char *path_prevMAC, char *path_curMAC, char *path_inputlog,
                       char *path_outputlog, guint32 bufsize,
                       enum LogMode logmode)
{
  guchar previousKey[KEY_LENGTH];
  guint64 previousKeyCounter = 0;

  msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Reading previous key file"), evt_tag_str("name", path_prevKey));
  gboolean success = readKey(previousKey, &previousKeyCounter, path_prevKey);
  if (!success)
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Previous key could not be loaded."), evt_tag_str("file",
                path_prevKey));
      return FALSE; //-- ERROR
    }

  msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Reading previous MAC file"), evt_tag_str("name", path_prevMAC));
  if ( ! g_file_test(path_prevMAC, G_FILE_TEST_IS_REGULAR))
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to readprevious MAC"), evt_tag_str("file", path_prevMAC));
      OPENSSL_cleanse(previousKey, sizeof previousKey);
      return FALSE; //-- ERROR
    }

  guchar previousMAC[CMAC_LENGTH];
  if (!readAggregatedMAC(path_prevMAC, previousMAC))
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to read previous MAC"), evt_tag_str("file", path_prevMAC));
      OPENSSL_cleanse(previousKey, sizeof previousKey);
      return FALSE; //-- ERROR
    }

  msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Reading current MAC file"), evt_tag_str("name", path_curMAC));
  if ( ! g_file_test(path_curMAC, G_FILE_TEST_IS_REGULAR))
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to read current MAC"), evt_tag_str("file", path_curMAC));
      OPENSSL_cleanse(previousKey, sizeof previousKey);
      OPENSSL_cleanse(previousMAC, sizeof previousMAC);
      return FALSE; //-- ERROR
    }

  guchar currentMAC[CMAC_LENGTH];
  if (!readAggregatedMAC(path_curMAC, currentMAC))
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to read current MAC"), evt_tag_str("file", path_curMAC));
      OPENSSL_cleanse(previousKey, sizeof previousKey);
      OPENSSL_cleanse(previousMAC, sizeof previousMAC);
      return FALSE; //-- ERROR
    }

  FILE *fp_input = fopen(path_inputlog, "r");
  if (fp_input == NULL)
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to open input log"), evt_tag_str("file", path_inputlog));
      OPENSSL_cleanse(previousKey, sizeof previousKey);
      OPENSSL_cleanse(previousMAC, sizeof previousMAC);
      OPENSSL_cleanse(currentMAC, sizeof currentMAC);
      return FALSE; //-- ERROR
    }

  // Scan through file ones
  guint64 entries = 0;
  int c;
  while ((c = fgetc(fp_input)) != EOF)
    {
      if (c == '\n')
        {
          entries++;
        }
    }

  fclose(fp_input);
  fp_input = NULL;

  msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Number of lines in file"), evt_tag_printf("number",
           "%" G_GUINT64_FORMAT, entries));
  msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Restoring and verifying log entries"), evt_tag_long("buffer size",
           bufsize));
  gboolean result = iterativeFileVerify(previousMAC,
                                        previousKey,
                                        path_inputlog,
                                        currentMAC,
                                        path_outputlog,
                                        entries,
                                        bufsize,
                                        previousKeyCounter,
                                        logmode);

  OPENSSL_cleanse(previousKey, sizeof previousKey);
  OPENSSL_cleanse(previousMAC, sizeof previousMAC);
  OPENSSL_cleanse(currentMAC, sizeof currentMAC);
  if (!result)
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason",
                                               "There is a problem with log verification. Please check log manually"));
      return FALSE; //-- ERROR
    }

  return TRUE; //-- SUCCESS
}


//
// main logic: 0 usually indicates success, and non-zero values indicate an error
//
int main(int argc, char *argv[])
{
  setlocale(LC_ALL, "");

  enum LogMode logmode = LOGMODE_ENCRYPTED;
  gint retval = 0; //-- 0: SUCCESS, main logic
  gboolean iterative = FALSE;
  guint32 bufSize = DEF_BUF_SIZE;


  if (TRUE == is_verbose)
    {
      g_print("ENTER slogverify: argc: %d\n", argc);
      for (int i = 0; i < argc; i++)
        {
          g_print("   argv[%d]: %s\n", i, argv[i]);
        }
    }

  SLogOptions options[] =
  {
    { "iterative", 'i', "Iterative verification", NULL, NULL },
    { "key-file", 'k', "Initial host key file", "FILE", NULL },
    { "mac-file", 'm', "Current MAC file", "FILE", NULL },
    { "logmode",  'l', "Log mode (direct|base64|enc) whether log is expected as is (direct) or plain but encoded as base64 or encrypted", "LOGMODE", NULL },
    { "prev-key-file", 'p', "Previous host key file in iterative mode", "FILE", NULL },
    { "prev-mac-file", 'r', "Previous MAC file in iterative mode", "FILE", NULL },
    { 0 }
  };

  GOptionEntry entries[] =
  {
    { options[0].longname, options[0].shortname, 0, G_OPTION_ARG_NONE, &iterative, options[0].description, NULL },
    { options[1].longname, options[1].shortname, 0, G_OPTION_ARG_CALLBACK, &validFileNameArg, options[1].description, options[1].type },
    { options[2].longname, options[2].shortname, 0, G_OPTION_ARG_CALLBACK, &validFileNameArg, options[2].description, options[2].type },
    { options[3].longname, options[3].shortname, 0, G_OPTION_ARG_CALLBACK, &validLogModeArg, options[3].description, options[3].type },
    { options[4].longname, options[4].shortname, 0, G_OPTION_ARG_CALLBACK, &validFileNameArg, options[4].description, options[4].type },
    { options[5].longname, options[5].shortname, 0, G_OPTION_ARG_CALLBACK, &validFileNameArg, options[5].description, options[5].type },
    { 0 }
  };

  GError *error = NULL;
  GOptionContext *context = g_option_context_new("INPUTLOG OUTPUTLOG [COUNTER] - Log archive verification\n\n" \
                                                 "Examples:\n" \
                                                 "  normal mode:\n" \
                                                 "    ./slogverify\n" \
                                                 "    --key-file ./host.key\n" \
                                                 "    --mac-file ./mac.dat\n" \
                                                 "    --logmode enc\n" \
                                                 "    ./messages.slog\n" \
                                                 "    ./messages_verified.txt\n\n"
                                                 "  iterative mode:\n" \
                                                 "    ./slogverify -i\n" \
                                                 "    --prev-key-file ./host0.key\n" \
                                                 "    --prev-mac-file ./mac0.dat\n" \
                                                 "    --mac-file ./mac1.dat\n" \
                                                 "    --logmode enc\n" \
                                                 "    ./plainlog_1.out\n" \
                                                 "    ./plainlog_1.chk\n"
                                                );

  GOptionGroup *group = g_option_group_new("Basic options", "Basic log archive verification options", "basic", &options,
                                           NULL);

  g_option_group_add_entries(group, entries);
  g_option_context_set_main_group(context, group);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      GString *errorMsg = g_string_new(error->message);
      (void) slog_usage(context, group, errorMsg);
      g_error_free(error);
      return 1; //-- ERROR
    }

  if (TRUE == is_verbose)
    {
      g_print("AFTER g_option_context_parse slogverify: argc: %d\n", argc);
      for (int i = 0; i < argc; i++)
        {
          g_print("   argv[%d]: %s\n", i, argv[i]);
        }
    }

  if ((argc < 2) || (argc > 4))
    {
      (void) slog_usage(context, group, NULL);
      return 1; //-- ERROR
    }

  // Initialize internal messaging
  msg_init(TRUE);

  GString *gstr_path_hostkey = g_string_new(NULL); //-- key-file
  GString *gstr_path_curMAC = g_string_new(NULL); //-- mac-file
  GString *gstr_path_prevhostkey = g_string_new(NULL); //-- prev-key-file
  GString *gstr_path_prevMAC = g_string_new(NULL); //-- prev-mac-file
  GString *gstr_path_inputlog = g_string_new(NULL); //-- INPUTLOG
  GString *gstr_path_outputlog = g_string_new(NULL); //-- OUTPUTLOG

  // Assign option arguments
  int optidx = 1;

  //-- key-file (hostkey), normal mode ---
  if (!iterative)
    {
      if (NULL == options[optidx].arg)
        {
          msg_error(SLOG_ERROR_PREFIX,
                    evt_tag_str("Reason",
                                "Option --key-file or -k does not provide a valid path to a key file!"));
          (void) slog_usage(context, group, NULL);
          context = NULL;
          retval = 1; //-- ERROR
          goto CLEANUP_SLOGVERIFY;
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
            goto CLEANUP_SLOGVERIFY;
          }
      }
      msg_info(SLOG_INFO_PREFIX, evt_tag_str("key-file", gstr_path_hostkey->str));
    }
  else
    {
      optidx++;
    }


  //-- mac-file, both iterative and normal mode argument
  if (NULL == options[optidx].arg)
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("Reason",
                            "Option --mac-file or -m does not provide a valid path to a mac file!"));
      (void) slog_usage(context, group, NULL);
      context = NULL;
      retval = 1; //-- ERROR
      goto CLEANUP_SLOGVERIFY;
    }
  {
    char *p_temp = g_strndup(options[optidx].arg, PATH_MAX - 1); //-- limit buffer
    g_free(options[optidx].arg);
    options[optidx++].arg = NULL; //-- inc
    char *p_canon = g_canonicalize_filename(p_temp, NULL); //-- normalize
    g_string_assign(gstr_path_curMAC, p_canon ? p_canon : "");
    g_free(p_temp);
    g_free(p_canon);
    if ((gstr_path_curMAC->len == 0U) ||
        (!is_file_path_safe_and_valid(gstr_path_curMAC->str)) ||
        (!g_file_test(gstr_path_curMAC->str, G_FILE_TEST_IS_REGULAR)))
      {
        msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "mac-file validation failed"));
        (void) slog_usage(context, group, NULL);
        context = NULL;
        retval = 1;
        goto CLEANUP_SLOGVERIFY;
      }
  }
  msg_info(SLOG_INFO_PREFIX, evt_tag_str("mac-file", gstr_path_curMAC->str));


  //-- logmode (direct|base64|enc) ---
  if (NULL == options[optidx].arg)
    {
      msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Old configuration without logmode: Use default enc"));
      logmode = LOGMODE_ENCRYPTED;
      optidx++; //-- inc
    }
  else
    {
      char *str_logmode_arg = g_strndup(options[optidx].arg, PATH_MAX - 1); //-- limit buffer
      g_free(options[optidx].arg);
      options[optidx++].arg = NULL; //-- inc
      logmode = convert_str_logmode(str_logmode_arg);
      g_free(str_logmode_arg);
      str_logmode_arg = NULL;
      if ( ((enum LogMode) LOGMODE_PLAIN_DIRECT != logmode) && ((enum LogMode)LOGMODE_PLAIN_BASE64 != logmode)
           && ((enum LogMode)LOGMODE_ENCRYPTED != logmode) )
        {
          msg_error(SLOG_ERROR_PREFIX,
                    evt_tag_str("Reason",
                                "Option --logmode or -l does not provide a valid logmode"));
          retval = 1; //-- ERROR
          goto CLEANUP_SLOGVERIFY;
        }
    }
  GString *gstr_logmode = convert_logmode_str(logmode);
  if (gstr_logmode)
    {
      msg_info(SLOG_INFO_PREFIX, evt_tag_str("logmode", gstr_logmode->str));
      g_string_free(gstr_logmode, TRUE);
      gstr_logmode = NULL;
    }

  //-- prev-key-file (prevhostkey), only iterative mode ---
  if (iterative)
    {
      if (NULL == options[optidx].arg)
        {
          msg_error(SLOG_ERROR_PREFIX,
                    evt_tag_str("Reason",
                                "Option --prev-key-file or -p does not provide a valid path to a prev key file!"));

          (void) slog_usage(context, group, NULL);
          context = NULL;
          retval = 1; //-- ERROR
          goto CLEANUP_SLOGVERIFY;
        }
      {
        char *p_temp = g_strndup(options[optidx].arg, PATH_MAX - 1); //-- limit buffer
        g_free(options[optidx].arg);
        options[optidx++].arg = NULL; //-- inc
        char *p_canon = g_canonicalize_filename(p_temp, NULL); //-- normalize
        g_string_assign(gstr_path_prevhostkey, p_canon ? p_canon : "");
        g_free(p_temp);
        g_free(p_canon);
        if ((gstr_path_prevhostkey->len == 0U) ||
            (!is_file_path_safe_and_valid(gstr_path_prevhostkey->str)) ||
            (!g_file_test(gstr_path_prevhostkey->str, G_FILE_TEST_IS_REGULAR)))
          {
            msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "prev-key-file validation failed"));
            (void) slog_usage(context, group, NULL);
            context = NULL;
            retval = 1;
            goto CLEANUP_SLOGVERIFY;
          }
      }
      msg_info(SLOG_INFO_PREFIX, evt_tag_str("prev-key-file", gstr_path_prevhostkey->str));
    }
  else
    {
      optidx++;
    }


  //-- prev-mac-file (prevMAC), only iterative mode ---
  if (iterative)
    {
      if (NULL == options[optidx].arg)
        {
          msg_error(SLOG_ERROR_PREFIX,
                    evt_tag_str("Reason",
                                "Option --prev-mac-file or -r does not provide valid path to a prev MAC file!"));

          (void) slog_usage(context, group, NULL);
          context = NULL;
          retval = 1; //-- ERROR
          goto CLEANUP_SLOGVERIFY;
        }
      {
        char *p_temp = g_strndup(options[optidx].arg, PATH_MAX - 1); //-- limit buffer
        g_free(options[optidx].arg);
        options[optidx++].arg = NULL; //-- inc
        char *p_canon = g_canonicalize_filename(p_temp, NULL); //-- normalize
        g_string_assign(gstr_path_prevMAC, p_canon ? p_canon : "");
        g_free(p_temp);
        g_free(p_canon);
        if ((gstr_path_prevMAC->len == 0U) ||
            (!is_file_path_safe_and_valid(gstr_path_prevMAC->str)) ||
            (!g_file_test(gstr_path_prevMAC->str, G_FILE_TEST_IS_REGULAR)))
          {
            msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "prev-mac-file validation failed"));
            (void) slog_usage(context, group, NULL);
            context = NULL;
            retval = 1;
            goto CLEANUP_SLOGVERIFY;
          }
      }
      msg_info(SLOG_INFO_PREFIX, evt_tag_str("prev-mac-file", gstr_path_prevMAC->str));
    }


  // Input and output file arguments
  optidx = 1;

  //-- INPUTLOG ---
  if (NULL == argv[optidx])
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Path to INPUTLOG is missing"));
      (void) slog_usage(context, group, NULL);
      context = NULL;
      retval = 1; //-- ERROR
      goto CLEANUP_SLOGVERIFY;
    }
  {
    char *p_temp = g_strndup(argv[optidx++], PATH_MAX - 1); //-- limit buffer, inc
    char *p_canon = g_canonicalize_filename(p_temp, NULL); //-- normalize
    g_string_assign(gstr_path_inputlog, p_canon ? p_canon : "");
    g_free(p_temp);
    g_free(p_canon);
    if ((gstr_path_inputlog->len == 0U) ||
        (!is_file_path_safe_and_valid(gstr_path_inputlog->str)) ||
        (!g_file_test(gstr_path_inputlog->str, G_FILE_TEST_IS_REGULAR)))
      {
        msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Check of INPUTLOG failed"));
        (void) slog_usage(context, group, NULL);
        context = NULL;
        retval = 1; //-- ERROR
        goto CLEANUP_SLOGVERIFY;
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
      goto CLEANUP_SLOGVERIFY;
    }
  {
    char *p_temp = g_strndup(argv[optidx++], PATH_MAX - 1); //-- limit buffer, inc
    char *p_canon = g_canonicalize_filename(p_temp, NULL); //-- normalize
    g_string_assign(gstr_path_outputlog, p_canon ? p_canon : "");
    g_free(p_temp);
    g_free(p_canon);
    if ((gstr_path_outputlog->len == 0U) ||
        (!is_file_path_safe_and_valid(gstr_path_outputlog->str))) //-- file might not exists yet
      {
        msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Check of OUTPUTLOG failed"));
        (void) slog_usage(context, group, NULL);
        context = NULL;
        retval = 1; //-- ERROR
        goto CLEANUP_SLOGVERIFY;
      }
  }
  msg_info(SLOG_INFO_PREFIX, evt_tag_str("OUTPUTLOG", gstr_path_outputlog->str));


  //-- Buffer size arguments if applicable ---
  if (TRUE == is_verbose)
    {
      g_print("Buffer counter when argc == 4: argc: %d\n", argc);
    }
  if (argc == 4)
    {
      char *endptr = NULL;
      long parsedVal;
      parsedVal = strtol(argv[optidx], &endptr, 10);
      if ((endptr == argv[optidx]) || (*endptr != '\0'))
        {
          msg_warning(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Failed strtol for buffer size. Default size is used instead!"),
                      evt_tag_long("Size", bufSize));
        }
      else
        {
          if ((parsedVal < 0) ||
              (parsedVal < (long)MIN_BUF_SIZE) ||
              (parsedVal > (long)MAX_BUF_SIZE))
            {
              msg_warning(SLOG_ERROR_PREFIX,
                          evt_tag_str("Reason", "Invalid buffer size. Default size is used instead!"),
                          evt_tag_long("Size", bufSize),
                          evt_tag_int("Minimum buffer size", MIN_BUF_SIZE),
                          evt_tag_int("Maximum buffer size", MAX_BUF_SIZE));
            }
          else
            {
              //-- value successfully parsed
              bufSize = (guint32)parsedVal;
            }
        }
      if (TRUE == is_verbose)
        {
          g_print("bufSize: %u\n", bufSize);
        }
    }

  if (iterative)
    {
      if ((gstr_path_prevhostkey->len == 0U) || (gstr_path_prevMAC->len == 0U) || (gstr_path_curMAC->len == 0U))
        {
          g_print("%s", g_option_context_get_help(context, TRUE, NULL));
          retval = 1; //-- ERROR
          goto CLEANUP_SLOGVERIFY;
        }
      gboolean result = iterativeMode(gstr_path_prevhostkey->str,
                                      gstr_path_prevMAC->str,
                                      gstr_path_curMAC->str,
                                      gstr_path_inputlog->str,
                                      gstr_path_outputlog->str,
                                      bufSize,
                                      logmode);
      if (!result)
        {
          retval = 1; //-- ERROR
        }
    }
  else
    {
      if ((gstr_path_hostkey->len == 0U) || (gstr_path_curMAC->len == 0U))
        {
          g_print("%s", g_option_context_get_help(context, TRUE, NULL));
          retval = 1; //-- ERROR
          goto CLEANUP_SLOGVERIFY;
        }
      gboolean result = normalMode(gstr_path_hostkey->str,
                                   gstr_path_curMAC->str,
                                   gstr_path_inputlog->str,
                                   gstr_path_outputlog->str,
                                   bufSize,
                                   logmode);
      if (!result)
        {
          retval = 1; //-- ERROR
        }
    }



CLEANUP_SLOGVERIFY:

  g_string_free(gstr_path_hostkey, TRUE);
  gstr_path_hostkey = NULL;
  g_string_free(gstr_path_curMAC, TRUE);
  gstr_path_curMAC = NULL;
  g_string_free(gstr_path_prevhostkey, TRUE);
  gstr_path_prevhostkey = NULL;
  g_string_free(gstr_path_prevMAC, TRUE);
  gstr_path_prevMAC = NULL;
  g_string_free(gstr_path_inputlog, TRUE);
  gstr_path_inputlog = NULL;
  g_string_free(gstr_path_outputlog, TRUE);
  gstr_path_outputlog = NULL;

  // Release messaging resources
  msg_deinit();

  if (NULL != context)
    {
      g_option_context_free(context);
      context = NULL;
    }

  return retval;
}
