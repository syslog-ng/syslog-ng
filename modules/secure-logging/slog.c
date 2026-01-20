/*
 * Copyright (c) 2019-2025 Airbus Commercial Aircraft
 * Copyright (c) 2024 Gergo Ferenc Kovacs
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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/cmac.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/params.h>
#endif

#include "messages.h"

#include "slog_sect.h"
#include "slog_file.h"
#include "slog.h"

// Argument indicators for command line utilities
#define LONG_OPT_INDICATOR "--"
#define SHORT_OPT_INDICATOR "-"

// This initialization only works with GCC.
static guchar KEYPATTERN[AES_BLOCKSIZE] = { [0 ... (AES_BLOCKSIZE - 1) ] = IPAD };
static guchar MACPATTERN[AES_BLOCKSIZE] = { [0 ... (AES_BLOCKSIZE - 1) ] = OPAD };
static guchar GAMMA[AES_BLOCKSIZE] = { [0 ... (AES_BLOCKSIZE - 1) ] =  EPAD};


// Retrieve counter from encrypted log entry
static gboolean getCounter(GString *entry, guint64 *logEntryOnDisk);

// Check whether value is contained in table
static gboolean tableContainsKey(GHashTable *table, guint64 value);

// Add new value to table
static gboolean addValueToTable(GHashTable *table, guint64 value);

// Get a single line from a log file
static GString *getLogEntry(SLogFile *f);

// Put a single line into a log file
static gboolean putLogEntry(SLogFile *f, GString *line);

// Clean up routine for GPtrArray
static void SLogStringFree(gpointer *arg);

/*
 * Create specific sub-keys for encryption and CMAC generation from key.
 *
 * 1. Parameter: (main) key (input)
 * 2. Parameter: encryption key (output)
 * 3. Parameter: (C)MAC key (output)
 *
 * Note: encKey and MACKey must have space to hold KEY_LENGTH many bytes.
 */
gboolean deriveSubKeys(guchar *mainKey, guchar *encKey, guchar *MACKey)
{
  return deriveEncSubKey(mainKey, encKey) && deriveMACSubKey(mainKey, MACKey);
}

gboolean deriveEncSubKey(guchar *mainKey, guchar *encKey)
{
  return PRF(mainKey, KEYPATTERN, sizeof(KEYPATTERN), encKey, KEY_LENGTH);
}

gboolean deriveMACSubKey(guchar *mainKey, guchar *MACKey)
{
  return PRF(mainKey, MACPATTERN, sizeof(MACPATTERN), MACKey, KEY_LENGTH);
}


// return TRUE on success, else FALSE
gboolean create_initial_mac0(guchar mainKey[KEY_LENGTH], guchar mac[CMAC_LENGTH])
{
  guchar encKey[KEY_LENGTH];
  guchar MACKey[KEY_LENGTH];
  memset(encKey, 0, G_N_ELEMENTS(encKey));
  memset(MACKey, 0, G_N_ELEMENTS(MACKey));

  if (!deriveSubKeys(mainKey, encKey, MACKey))
    {
      return FALSE;
    }
  gsize outlen = 0;

  // This buffer holds everything: AggregatedMAC, IV, Tag, and CText
  // Binary data cannot be larger than its base64 encoding
  guchar bigBuf[AES_BLOCKSIZE + IV_LENGTH + AES_BLOCKSIZE];
  gsize nb = G_N_ELEMENTS(bigBuf);
  memset(bigBuf, 0, nb);

  // This is where are ciphertext related data starts
  guchar *ctBuf = &bigBuf[AES_BLOCKSIZE];
  guchar *iv = ctBuf;

  guchar outputBigMac[CMAC_LENGTH];
  gsize outputBigMac_capacity = G_N_ELEMENTS(outputBigMac);
  memset(outputBigMac, 0, outputBigMac_capacity);

  // Generate random nonce
  if (RAND_bytes(iv, IV_LENGTH) == 1)
    {
      if (!cmac(MACKey, &bigBuf[AES_BLOCKSIZE], IV_LENGTH + AES_BLOCKSIZE, outputBigMac, &outlen,
                outputBigMac_capacity))
        {
          msg_error(SLOG_ERROR_PREFIX,
                    evt_tag_str("File: ", __FILE__),
                    evt_tag_long("Line: ", __LINE__),
                    evt_tag_long("Code: ", SLOG_MEM_ALLOC_ERROR),
                    evt_tag_str("Reason: ", "Bad CMAC")
                   );
          return FALSE;
        }
      memcpy(mac, outputBigMac, CMAC_LENGTH);
      msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "MAC0 has been created"));
    }
  return TRUE;
}



/**
 * Gets the path to mac0.dat based on the directory of pathAggMac.
 * Returns TRUE on success, FALSE otherwise.
 */
gboolean get_path_mac0(const gchar *pathAggMac, gchar *pathMac0, size_t sizePathMac0)
{
  gboolean retval = FALSE;
  if (pathAggMac == NULL || pathMac0 == NULL || sizePathMac0 == 0)
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Invalid path: pathAggMac or pathMac0 or sizePathMac0"));
      return retval;
    }
  gchar *dirname = g_path_get_dirname(pathAggMac);
  //-- Check if dirname is valid and actually exists on the system
  if (dirname != NULL && g_file_test(dirname, G_FILE_TEST_IS_DIR))
    {
      //-- Build the full path using GLib (handles '/' automatically)
      gchar *full_path = g_build_filename(dirname, "mac0.dat", NULL);
      if (full_path != NULL)
        {
          //-- Safely copy to the output buffer
          if (strlen(full_path) < sizePathMac0)
            {
              g_strlcpy(pathMac0, full_path, sizePathMac0);
              retval = TRUE;
            }
          else
            {
              msg_warning(SLOG_WARNING_PREFIX, evt_tag_str("Reason", "Destination buffer too small for path"));
            }
          g_free(full_path);
        }
    }
  else
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("Reason", "Directory does not exist!"),
                evt_tag_str("Path", dirname)); //-- dirname NULL is handled
    }
  g_free(dirname);
  return retval;
}


/*
 * AES256-GCM encryption
 *
 * Encrypts plaintext
 *
 * 1. Parameter: pointer to plaintext (input)
 * 2. Parameter: length of plaintext (input)
 * 3. Parameter: pointer to key (input)
 * 4. Parameter: pointer to IV (input, nonce of length IV_LENGTH)
 * 5. Parameter: pointer to ciphertext (output)
 * 6. Parameter: pointer to tag (output)
 *
 * Note: caller must take care of memory management.
 *
 * Return:
 * Length of ciphertext (>0)
 * 0 on error
 */
int sLogEncrypt(guchar *plaintext, int plaintext_len,
                guchar *key, guchar *iv,
                guchar *ciphertext, guchar *tag)
{
  /*
   * This function is largely borrowed from
   *
   * https://wiki.openssl.org/index.php/EVP_Authenticated_Encryption_and_Decryption#Authenticated_Encryption_using_GCM_mode
   *
   */
  EVP_CIPHER_CTX *ctx;

  int len;
  int ciphertext_len;

  /* Create and initialise the context */
  if ( !(ctx = EVP_CIPHER_CTX_new()) )
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to initialize OpenSSL contex"));
      return 0;
    }

  /* Initialise the encryption operation. */
  if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL))
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to initialize OpenSSL contex"));
      return 0;
    }

  if (IV_LENGTH != 12)
    {
      /* Set IV length if default 12 bytes (96 bits) is not appropriate */
      if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_LENGTH, NULL))
        {
          msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to set IV length"));
          return 0;
        }
    }

  /* Initialise key and IV */
  if (1 != EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv))
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to initialize encryption key and IV"));
      return 0;
    }

  /* Provide the message to be encrypted, and obtain the encrypted output.
   * EVP_EncryptUpdate can be called multiple times if necessary
   */
  if (1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len))
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to encrypt data"));
      return 0;
    }

  ciphertext_len = len;

  /* Finalise the encryption. Normally ciphertext bytes may be written at
   * this stage, but this does not occur in GCM mode
   */
  if (1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len))
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to complete encryption of data"));
      return 0;
    }

  ciphertext_len += len;

  /* Get the tag */
  if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, AES_BLOCKSIZE, tag))
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to acquire encryption tag"));
      return 0;
    }

  /* Clean up */
  EVP_CIPHER_CTX_free(ctx);

  return ciphertext_len;
}

/*
 * Decrypt ciphertext and verify integrity
 *
 * 1. Parameter: Pointer to ciphertext (input)
 * 2. Parameter: Ciphertext length (input)
 * 3. Parameter: Pointer to integrity tag (input)
 * 4. Parameter: Pointer to IV (input)
 * 5. Parameter: Pointer to plaintext (output)
 *
 * Note: Caller must take care of memory management.
 *
 * Return:
 * >0 success
 * -1 in case verification fails
 * 0 on error
 */
int sLogDecrypt(guchar *ciphertext,
                int ciphertext_len,
                guchar *tag,
                guchar *key,
                guchar *iv,
                guchar *plaintext)
{
  EVP_CIPHER_CTX *ctx;
  int len;
  int plaintext_len;
  int ret;

  /* Create and initialise the context */
  if (!(ctx = EVP_CIPHER_CTX_new()))
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to initialize OpenSSL context"));
      return 0;
    }

  /* Initialise the decryption operation. */
  if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL))
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable initiate decryption operation"));
      return 0;
    }

  if (IV_LENGTH != 12)
    {
      /* Set IV length. Not necessary if this is 12 bytes (96 bits) */
      if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_LENGTH, NULL))
        {
          msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable set IV length"));
          return 0;
        }
    }

  /* Initialise key and IV */
  if (!EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv))
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to initialize key and IV"));
      return 0;
    }

  /* Provide the message to be decrypted, and obtain the plaintext output.
   * EVP_DecryptUpdate can be called multiple times if necessary
   */
  if (!EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len))
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to decrypt"));
      return 0;
    }

  plaintext_len = len;

  /* Set expected tag value. Works in OpenSSL 1.0.1d and later */
  if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, AES_BLOCKSIZE, tag))
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable set tag value"));
      return 0;
    }

  /* Finalise the decryption. A positive return value indicates success,
   * anything else is a failure - the plaintext is not trustworthy.
   */
  ret = EVP_DecryptFinal_ex(ctx, plaintext + len, &len);

  /* Clean up */
  EVP_CIPHER_CTX_free(ctx);
  if (ret > 0)
    {
      /* Success */
      plaintext_len += len;
      return plaintext_len;
    }
  else
    {
      /* Verify failed */
      return -1;
    }
}

/*
 *  Create new forward-secure log entry
 *
 * This function creates a new encrypted log entry updates the corresponding MAC accordingly
 *
 * 1. Parameter: Number of log entries (for enumerating the entries in the log file)
 * 2. Parameter: The original log message
 * 3. Parameter: The current key
 * 4. Parameter: The current MAC
 * 5. Parameter: The resulting encrypted log entry
 * 6. Parameter: The newly updated MAC
 * 7. Parameter: The capacity of the newly updated MAC buffer
*/
gboolean sLogEntry(
  guint64 numberOfLogEntries,
  GString *text,
  guchar *mainKey,
  guchar *inputBigMac,
  GString *output,
  guchar *outputBigMac,
  gsize outputBigMac_capacity)
{
  guchar encKey[KEY_LENGTH];
  guchar MACKey[KEY_LENGTH];

  //-- according sub functions mainKey is also  most likely of length KEY_LENGTH
  if (!deriveSubKeys(mainKey, encKey, MACKey))
    {
      return FALSE;
    }

  // Compute current log entry number
  gchar *counterString = g_base64_encode((const guchar *)&numberOfLogEntries, sizeof(numberOfLogEntries));

  int slen = (int) text->len;

  // This buffer holds everything: AggregatedMAC, IV, Tag, and CText
  // Binary data cannot be larger than its base64 encoding
  guchar bigBuf[AES_BLOCKSIZE + IV_LENGTH + AES_BLOCKSIZE + slen];

  // This is where are ciphertext related data starts
  guchar *ctBuf = &bigBuf[AES_BLOCKSIZE];
  guchar *iv = ctBuf;
  guchar *tag = &bigBuf[AES_BLOCKSIZE + IV_LENGTH];
  guchar *ciphertext = &bigBuf[AES_BLOCKSIZE + IV_LENGTH + AES_BLOCKSIZE];

// Generate random nonce
  if (RAND_bytes(iv, IV_LENGTH) == 1)
    {
      // Encrypt log data
      int ct_length = sLogEncrypt((guchar *)text->str, slen, encKey, iv, ciphertext, tag);
      if (ct_length <= 0)
        {
          msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Unable to correctly encrypt log message"));
          g_string_printf(output, "%*.*s:%s: %s", COUNTER_LENGTH, COUNTER_LENGTH, counterString,
                          SLOG_ERROR_PREFIX ": Unable to correctly encrypt the following log message:", text->str);
          g_free(counterString);
          return FALSE;
        }

      // Write current log entry number
      g_string_printf (output, "%*.*s:", COUNTER_LENGTH, COUNTER_LENGTH, counterString);
      g_free(counterString);

      // Write IV, tag, and ciphertext at once
      gchar *encodedCtBuf = g_base64_encode(ctBuf, IV_LENGTH + AES_BLOCKSIZE + ct_length);
      g_string_append(output, encodedCtBuf);
      g_free(encodedCtBuf);

      // Compute aggregated MAC
      gsize outlen = 0;
      //-- The initial MAC file has been created, so there is no need
      //   anymore to check whether this is the very first encryption.
      memcpy(bigBuf, inputBigMac, AES_BLOCKSIZE);
      if (!cmac(MACKey, bigBuf, AES_BLOCKSIZE + IV_LENGTH + AES_BLOCKSIZE + ct_length, outputBigMac, &outlen,
                outputBigMac_capacity))
        {
          msg_error(SLOG_ERROR_PREFIX,
                    evt_tag_str("File: ", __FILE__),
                    evt_tag_long("Line: ", __LINE__),
                    evt_tag_long("Code: ", SLOG_MEM_ALLOC_ERROR),
                    evt_tag_str("Reason: ", "Bad CMAC")
                   );
          return FALSE;
        }
    }
  else
    {
      // We did not get enough random bytes
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Could not obtain enough random bytes"));
      g_string_printf(output, "%*.*s:%s: %s", COUNTER_LENGTH, COUNTER_LENGTH, counterString,
                      SLOG_ERROR_PREFIX ": Could not obtain enough random bytes for the following log message:", text->str);
      g_free(counterString);
      return FALSE;
    }
  return TRUE;
}

/*
 * Evolve key multiple times
 *
 * 1. Parameter: Pointer to destination key (output)
 * 2. Parameter: Number of times current key should be evolved (input)
 * 3. Parameter: Pointer to current key (input)
 *
 * Note: Caller must take care of memory management.
 *
 */
gboolean deriveKey(guchar *dst, guint64 index, guint64 currentKey)
{
  gboolean result = TRUE;

  for (guint64 i = currentKey; i < index; i++)
    {
      result = evolveKey(dst);
    }
  return result;
}

/*
 * Compute AES256 CMAC of input
 *
 * 1. Parameter: Pointer to key (input)
 * 2. Parameter: Pointer to input (input)
 * 3. Parameter: Input length (input)
 * 4. Parameter: Pointer to output (output)
 * 5. Parameter: Length of output (output)
 * 6. Parameter: Capacity of output buffer (input)
 *
 * Note: caller must take care of memory management.
 *
 * If Parameter 5 == 0, there was an error.
 *
 * Return:
 *   TRUE on success
 *   FALSE on error
 *
 */
gboolean cmac(guchar *key, const void *input,
              gsize length, guchar *out,
              gsize *outlen, gsize out_capacity)
{
  size_t output_len;

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  EVP_MAC *mac = EVP_MAC_fetch(NULL, "CMAC", NULL);
  if (mac == NULL)
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("File: ", __FILE__),
                evt_tag_long("Line: ", __LINE__),
                evt_tag_long("Code: ", SLOG_MEM_ALLOC_ERROR),
                evt_tag_str("Reason: ", "EVP_MAC_fetch() returned NULL")
               );
      return FALSE;
    }

  OSSL_PARAM params[] =
  {
    OSSL_PARAM_utf8_string("cipher", "aes-256-cbc", 0),
    OSSL_PARAM_END,
  };

  EVP_MAC_CTX *ctx = EVP_MAC_CTX_new(mac);
  if (ctx == NULL)
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("File: ", __FILE__),
                evt_tag_long("Line: ", __LINE__),
                evt_tag_long("Code: ", SLOG_MEM_ALLOC_ERROR),
                evt_tag_str("Reason: ", "EVP_MAC_CTX_new() returned NULL")
               );

      EVP_MAC_free(mac);
      return FALSE;
    }

  if (EVP_MAC_init(ctx, key, KEY_LENGTH, params) == 0)
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("File: ", __FILE__),
                evt_tag_long("Line: ", __LINE__),
                evt_tag_long("Code: ", SLOG_OPENSSL_LIBRARY_ERROR),
                evt_tag_str("Reason: ", "EVP_MAC_init() returned 0")
               );
      EVP_MAC_CTX_free(ctx);
      EVP_MAC_free(mac);
      return FALSE;
    }

  if (EVP_MAC_update(ctx, input, length) == 0)
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("File: ", __FILE__),
                evt_tag_long("Line: ", __LINE__),
                evt_tag_long("Code: ", SLOG_OPENSSL_LIBRARY_ERROR),
                evt_tag_str("Reason: ", "EVP_MAC_update() returned 0")
               );
      EVP_MAC_CTX_free(ctx);
      EVP_MAC_free(mac);
      return FALSE;
    }

  if (EVP_MAC_final(ctx, out, &output_len, out_capacity) == 0)
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("File: ", __FILE__),
                evt_tag_long("Line: ", __LINE__),
                evt_tag_long("Code: ", SLOG_OPENSSL_LIBRARY_ERROR),
                evt_tag_str("Reason: ", "EVP_MAC_final() returned 0")
               );
      EVP_MAC_CTX_free(ctx);
      EVP_MAC_free(mac);
      return FALSE;
    }

  EVP_MAC_CTX_free(ctx);
  EVP_MAC_free(mac);
#else
  CMAC_CTX *ctx = CMAC_CTX_new();
  if (ctx == NULL)
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("File: ", __FILE__),
                evt_tag_long("Line: ", __LINE__),
                evt_tag_long("Code: ", SLOG_MEM_ALLOC_ERROR),
                evt_tag_str("Reason: ", "CMAC_CTX_new() returned NULL")
               );
      return FALSE;
    }

  if (CMAC_Init(ctx, key, KEY_LENGTH, EVP_aes_256_cbc(), NULL) == 0)
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("File: ", __FILE__),
                evt_tag_long("Line: ", __LINE__),
                evt_tag_long("Code: ", SLOG_OPENSSL_LIBRARY_ERROR),
                evt_tag_str("Reason: ", "CMAC_Init() returned 0")
               );
      CMAC_CTX_free(ctx);
      return FALSE;
    }

  if (CMAC_Update(ctx, input, length) == 0)
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("File: ", __FILE__),
                evt_tag_long("Line: ", __LINE__),
                evt_tag_long("Code: ", SLOG_OPENSSL_LIBRARY_ERROR),
                evt_tag_str("Reason: ", "CMAC_Update() returned 0")
               );
      CMAC_CTX_free(ctx);
      return FALSE;
    }

  if (CMAC_Final(ctx, out, &output_len) == 0)
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("File: ", __FILE__),
                evt_tag_long("Line: ", __LINE__),
                evt_tag_long("Code: ", SLOG_OPENSSL_LIBRARY_ERROR),
                evt_tag_str("Reason: ", "CMAC_Final() returned 0")
               );
      CMAC_CTX_free(ctx);
      return FALSE;
    }
  CMAC_CTX_free(ctx);
#endif

  // CMAC length must be 16 bytes
  if (output_len != CMAC_LENGTH)
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("File: ", __FILE__),
                evt_tag_long("Line: ", __LINE__),
                evt_tag_long("Code: ", SLOG_OPENSSL_LIBRARY_ERROR),
                evt_tag_str("Reason: ", "CMAC output incorrect"),
                evt_tag_long("Expected bytes: ", CMAC_LENGTH),
                evt_tag_long("Got bytes: ", output_len)
               );
      return FALSE;
    }

  *outlen = (gsize)output_len;

  return TRUE;
}

/*
 *  Evolve key
 *
 * 1. Parameter: Pointer to key (input/output)
 *
 */
gboolean evolveKey(guchar *key)
{
  guchar buf[KEY_LENGTH];
  if (PRF(key, GAMMA, sizeof(GAMMA), buf, KEY_LENGTH))
    {
      memcpy(key, buf, KEY_LENGTH);
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

/*
 * AES-CMAC based pseudo-random function (with variable input length and variable output length)
 *
 * https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-56Cr2.pdf
 * https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-108r1-upd1.pdf
 *
 * 1. Parameter: Pointer to key (input)
 * 2. Parameter: Pointer to input (input)
 * 3. Parameter: Length of input (input)
 * 4. Parameter: Pointer to output (output)
 * 5. Parameter: Required output length (input)
 *
 * Note: For security reasons, outputLength must be less than 255 * AES_BLOCKSIZE
 *
 * Return:
 *  TRUE on success
 *  FALSE on error
 *
 */
gboolean PRF(guchar *key, guchar *originalInput,
             guint64 originalInputLength, guchar *output,
             guint64 outputLength)
{
  // First, extraction
  guchar ktmp[KEY_LENGTH];
  // Initialize to all zero, in case CMAC_LENGTH < KEY_LENGTH
  memset(ktmp, 0, KEY_LENGTH);
  gsize outlen = -1;

  // Assume KEY_LENGTH >= CMAC_LENGTH
  if (!cmac(key, originalInput, originalInputLength, ktmp, &outlen, CMAC_LENGTH))
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("Reason", "Bad CMAC"),
                evt_tag_str("File: ", __FILE__),
                evt_tag_long("Line: ", __LINE__));
      return FALSE;
    }

  // Then, expansion
  gsize n = outputLength / CMAC_LENGTH;

  gchar label[] = "key-expansion";
  gchar context[] = "context";

  size_t label_len = strlen(label);
  size_t context_len = strlen(context);
  size_t output_len = sizeof(outputLength);
  size_t gsize_len = sizeof(gsize);

  // i || Label || 00 || Context || outputLength;
  gsize myInputLength = sizeof(gsize) + label_len + 1 + context_len + output_len;

  guchar *input = g_new0(guchar, myInputLength);

  for (gsize i = 0 ; i < n ; i++)
    {
      // input = i || Label || 00 || Context || outputLength;
      memcpy(input, &i, gsize_len);
      memcpy(input + gsize_len, label, label_len);
      input[gsize_len + label_len] = 0;

      memcpy(input + gsize_len + label_len + 1, context, context_len);
      memcpy(input + gsize_len + label_len + 1 + context_len, &outputLength, output_len);

      if (!cmac(ktmp, input, myInputLength, output + i * CMAC_LENGTH, &outlen, CMAC_LENGTH))
        {
          msg_error(SLOG_ERROR_PREFIX,
                    evt_tag_str("Reason", "Bad CMAC"),
                    evt_tag_str("File: ", __FILE__),
                    evt_tag_long("Line: ", __LINE__));
          g_free(input);
          return FALSE;
        }

    }

  if (outputLength % CMAC_LENGTH != 0)
    {
      guchar buf[CMAC_LENGTH];

      memcpy (input, &n, gsize_len);
      memcpy(input + gsize_len, label, label_len);
      input[gsize_len + label_len] = 0;
      memcpy(input + gsize_len + label_len + 1, context, context_len);
      memcpy(input + gsize_len + label_len + 1 + context_len, &outputLength, output_len);

      if (!cmac(ktmp, input, myInputLength, buf, &outlen, CMAC_LENGTH))
        {
          msg_error(SLOG_ERROR_PREFIX,
                    evt_tag_str("Reason", "Bad CMAC"),
                    evt_tag_str("File: ", __FILE__),
                    evt_tag_long("Line: ", __LINE__));
          g_free(input);
          return FALSE;
        }

      memcpy(output + n * CMAC_LENGTH, buf, outputLength % CMAC_LENGTH);
    }
  g_free(input);
  return TRUE;
}

/*
 * Generate a master key
 *
 * This unique master key requires 32 bytes of storage.
 * The caller has to allocate this memory.
 *
 * Return:
 * TRUE on success
 * FALSE on error
 */
gboolean generateMasterKey(guchar *masterkey)
{
  return RAND_bytes(masterkey, KEY_LENGTH) == 1 ? TRUE : FALSE;
}

/*
 * Generate a host key based on a previously created master key
 *
 * 1. Parameter: master key (input)
 * 2. Parameter: Host MAC address (input)
 * 3. Parameter: Host S/N (input)
 * 4. Parameter: Host key (output)
 *
 * The specific unique host key k_0 is k_0 = PRF_{master_key}(MAC address ||
 * S/N) and requires 32 bytes of storage. The caller has to allocate this
 * memory.
 */

gboolean deriveHostKey(guchar *masterkey, gchar *macAddr, gchar *serial, guchar *hostkey)
{
  gchar concatString[strlen(macAddr) + strlen(serial) + 1];
  concatString[0] = 0;
  strncat(concatString, macAddr, sizeof(concatString) - strlen(concatString) - 1);
  strncat(concatString, serial, sizeof(concatString) - strlen(concatString) - 1);
  return PRF(masterkey, (guchar *) concatString, strlen(concatString), hostkey, KEY_LENGTH);
}


/*
 *  Write whole log MAC to file
 *
 * Return:
 * TRUE on success
 * FALSE on error
 */
gboolean writeAggregatedMAC(gchar *filename, guchar *outputBuffer)
{
  SLogFile *f = create_file(filename, "w+");

  if (f == NULL)
    {
      return FALSE;
    }

  gboolean volatile result = TRUE;
  gboolean volatile cmacOk = TRUE;

  SLOG_SECT_START(f)
  result = open_file(f);
  if (!result)
    {
      SLogSectForward(__FILE__, __LINE__, f->state, handleFileError);
    }

  // Write aggregated MAC
  result = write_to_file(f, (gchar *) outputBuffer, CMAC_LENGTH);
  if (!result)
    {
      SLogSectForward(__FILE__, __LINE__, f->state, handleFileError);
    }

  // Compute new aggregated MAC
  gchar outputmacdata[CMAC_LENGTH];
  gsize outlen;
  gsize outputmacdata_capacity = G_N_ELEMENTS(outputmacdata);
  guchar keyBuffer[KEY_LENGTH];
  memset(keyBuffer, 0, KEY_LENGTH);
  guchar zeroBuffer[CMAC_LENGTH];
  memset(zeroBuffer, 0, CMAC_LENGTH);
  memcpy(keyBuffer, outputBuffer, MIN(CMAC_LENGTH, KEY_LENGTH));

  if (!cmac(keyBuffer, zeroBuffer, CMAC_LENGTH, (guchar *)outputmacdata, &outlen, outputmacdata_capacity))
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Bad CMAC"));
      cmacOk = FALSE;
    }

  // Write new aggregated MAC to file
  result = write_to_file(f, outputmacdata, CMAC_LENGTH);
  if (!result)
    {
      SLogSectForward(__FILE__, __LINE__, f->state, handleFileError);
    }

  // Close file
  result = close_file(f);
  if (!result)
    {
      SLogSectForward(__FILE__, __LINE__, f->state, handleFileError);
    }
  SLOG_SECT_END(f)
  g_free(f);

  return result && cmacOk;
}

/*
 * Read whole log MAC from file
 *
 * Return:
 * TRUE on success
 * FALSE on error
 */
gboolean readAggregatedMAC(gchar *filename, guchar *outputBuffer)
{
  SLogFile *f = create_file(filename, "r");

  if (f == NULL)
    {
      return FALSE;
    }

  gboolean volatile result = TRUE;
  gboolean volatile cmacOk = TRUE;
  gchar macdata[2 * CMAC_LENGTH];

  // If file does not exist there is nothing to do
  if (!g_file_test(filename, G_FILE_TEST_IS_REGULAR))
    {
      msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "MAC does not yet exist and will be created"));
      close_file(f);
      return TRUE;
    }


  SLOG_SECT_START(f)
  result = open_file(f);
  if (!result)
    {
      SLogSectForward(__FILE__, __LINE__, f->state, handleFileError);
    }

  result = read_from_file(f, macdata, 2 * CMAC_LENGTH);
  if (!result)
    {
      SLogSectForward(__FILE__, __LINE__, f->state, handleFileError);
    }

  gsize outlen = 0;
  guchar keyBuffer[KEY_LENGTH];
  memset(keyBuffer, 0, KEY_LENGTH);
  guchar zeroBuffer[CMAC_LENGTH];
  memset(zeroBuffer, 0, CMAC_LENGTH);
  memcpy(keyBuffer, macdata, MIN(CMAC_LENGTH, KEY_LENGTH));

  guchar testOutput[CMAC_LENGTH];
  gsize testOutput_capacity = G_N_ELEMENTS(testOutput);

  if (!cmac(keyBuffer, zeroBuffer, CMAC_LENGTH, testOutput, &outlen, testOutput_capacity))
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("File: ", __FILE__),
                evt_tag_long("Line: ", __LINE__),
                evt_tag_str("Reason", "Bad CMAC"));
      cmacOk = FALSE;
    }

  if (0 != memcmp(testOutput, &macdata[CMAC_LENGTH], CMAC_LENGTH))
    {
      msg_warning(SLOG_WARNING_PREFIX, evt_tag_str("Reason", "MAC computation invalid"));
      cmacOk = FALSE;
    }
  else
    {
      msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "MAC successfully loaded"));
    }

  result = close_file(f);
  if (!result)
    {
      SLogSectForward(__FILE__, __LINE__, f->state, handleFileError);
    }

  memcpy(outputBuffer, macdata, CMAC_LENGTH);

  SLOG_SECT_END(f)
  g_free(f);

  return result && cmacOk;
}

/*
 *  Read key from file
 *
 * Return:
 * TRUE on success
 * FALSE on error
 */
gboolean readKey(guchar *destKey, guint64 *destCounter, gchar *keypath)
{
  SLogFile *f = create_file(keypath, "r");

  if (f == NULL)
    {
      return FALSE;
    }

  gboolean volatile result = TRUE;
  gboolean volatile cmacOk = TRUE;
  gchar keydata[KEY_LENGTH + CMAC_LENGTH];
  guint64 littleEndianCounter;

  SLOG_SECT_START(f)
  result = open_file(f);
  if (!result)
    {
      SLogSectForward(__FILE__, __LINE__, f->state, handleFileError);
    }

  // Key file contains
  // 1. The number of log entries already logged (and therefore the
  //    index of the key). This is at the end of the key file
  // 2. The actual 32 byte key stored this is at the beginning of the key file
  // 3. A 16 byte CMAC of the two previous data stored after the actual key

  result = read_from_file(f, keydata, KEY_LENGTH + CMAC_LENGTH);
  if (!result)
    {
      SLogSectForward(__FILE__, __LINE__, f->state, handleFileError);
    }

  result = read_from_file(f, (gchar *)&littleEndianCounter, sizeof(littleEndianCounter));
  if (!result)
    {
      SLogSectForward(__FILE__, __LINE__, f->state, handleFileError);
    }

  gsize outlen = 0;
  guchar testOutput[CMAC_LENGTH];
  gsize testOutputCapacity = G_N_ELEMENTS(testOutput);

  if (!cmac((guchar *)keydata, &(littleEndianCounter), sizeof(littleEndianCounter), testOutput, &outlen,
            testOutputCapacity))
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("File: ", __FILE__),
                evt_tag_long("Line: ", __LINE__),
                evt_tag_str("Reason", "Bad CMAC"));
      cmacOk = FALSE;
    }

  if (0 != memcmp(testOutput, &keydata[KEY_LENGTH], CMAC_LENGTH))
    {
      msg_warning(SLOG_WARNING_PREFIX, evt_tag_str("Reason", "Host key corrupted. CMAC in key file not matching"));
      result = FALSE;
    }

  // Close file
  result = close_file(f);
  if (!result)
    {
      SLogSectForward(__FILE__, __LINE__, f->state, handleFileError);
    }

  SLOG_SECT_END(f)

  memcpy(destKey, keydata, KEY_LENGTH);
  *destCounter = GUINT64_FROM_LE(littleEndianCounter);
  g_free(f);

  return result && cmacOk;
}

/*
 * Write key to file
 *
 * Return:
 * TRUE on success
 * FALSE on error
 */
gboolean writeKey(guchar *key, guint64 counter, gchar *keypath)
{
  SLogFile *f = create_file(keypath, "w+");

  if (f == NULL)
    {
      return FALSE;
    }

  gboolean volatile result = TRUE;
  gboolean volatile cmacOk = TRUE;

  SLOG_SECT_START(f)
  result = open_file(f);
  if (!result)
    {
      SLogSectForward(__FILE__, __LINE__, f->state, handleFileError);
    }

  // Write key
  result = write_to_file(f, (gchar *) key, KEY_LENGTH);
  if (!result)
    {
      SLogSectForward(__FILE__, __LINE__, f->state, handleFileError);
    }

  guint64 littleEndianCounter = GINT64_TO_LE(counter);
  gchar outputmacdata[CMAC_LENGTH];
  gsize outputmacdata_capacity = G_N_ELEMENTS(outputmacdata);
  gsize outlen = 0;

  // Create CMAC for key
  if (!cmac((guchar *)key, &littleEndianCounter, sizeof(littleEndianCounter),
            (guchar *)outputmacdata, &outlen, outputmacdata_capacity))
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("File: ", __FILE__),
                evt_tag_long("Line: ", __LINE__),
                evt_tag_str("Reason", "Bad CMAC"));
      cmacOk = FALSE;
    }

  // Write CMAC
  result = write_to_file(f, outputmacdata, CMAC_LENGTH);
  if (!result)
    {
      SLogSectForward(__FILE__, __LINE__, f->state, handleFileError);
    }

  // Write counter
  result = write_to_file(f, (gchar *)&littleEndianCounter,  sizeof(littleEndianCounter));
  if (!result)
    {
      SLogSectForward(__FILE__, __LINE__, f->state, handleFileError);
    }

  result = close_file(f);
  if (!result)
    {
      SLogSectForward(__FILE__, __LINE__, f->state, handleFileError);
    }

  SLOG_SECT_END(f)
  g_free(f);

  return result && cmacOk;
}

// Iterate through log entries contained in a buffer and verify them
gboolean iterateBuffer(
  guint64 entriesInBuffer,
  GPtrArray *input,
  guint64 *nextLogEntry,
  guchar *mainKey,
  guchar *keyZero,
  guint keyNumber,
  GPtrArray *output,
  guint64 *numberOfLogEntries,
  guchar *cmac_tag,
  gsize cmac_tag_capacity,
  GHashTable *tab)
{
  gboolean result = TRUE;
  for (guint64 i = 0; i < entriesInBuffer; i++)
    {
      g_ptr_array_add(output, g_string_new(NULL));

      GString *entry = (GString *)g_ptr_array_index(input, i);
      guint64 len = entry->len;
      guint64 logEntryOnDisk;

      if (len > (COUNTER_LENGTH + 1))
        {
          if (!getCounter(entry, &logEntryOnDisk))
            {
              // Cannot determine counter value -> Jump to next entry
              logEntryOnDisk = *nextLogEntry;
            }

          // Subtract counter from log entry
          len = len - (COUNTER_LENGTH + 1);

          if (logEntryOnDisk != *nextLogEntry)
            {
              if (tableContainsKey(tab, logEntryOnDisk))
                {
                  msg_error(SLOG_ERROR_PREFIX,
                            evt_tag_str("Reason", "Duplicate entry detected"),
                            evt_tag_long("entry", logEntryOnDisk));
                  result = FALSE;
                }
              if (logEntryOnDisk < (*nextLogEntry))
                {
                  if (logEntryOnDisk < keyNumber)
                    {
                      msg_error(SLOG_ERROR_PREFIX,
                                evt_tag_str("Reason",
                                            "Log claims to be past entry from past archive. We cannot rewind back to this key without key0. This is going to fail"),
                                evt_tag_long("entry", logEntryOnDisk));
                      result = FALSE;
                    }
                  else
                    {
                      msg_error(SLOG_ERROR_PREFIX,
                                evt_tag_str("Reason", "Log claims to be past entry. We rewind from first known key, this might take some time"),
                                evt_tag_long("entry", logEntryOnDisk));
                      // Rewind key to k0
                      memcpy(mainKey, keyZero, KEY_LENGTH);
                      deriveKey(mainKey, logEntryOnDisk, keyNumber);
                      *nextLogEntry = logEntryOnDisk;
                      result = FALSE;
                    }
                }
              if (logEntryOnDisk - (*nextLogEntry) > 1000000)
                {
                  msg_info(SLOG_INFO_PREFIX,
                           evt_tag_str("Reason", "Deriving key for distant future. This might take some time."),
                           evt_tag_long("next log entry should be", *nextLogEntry),
                           evt_tag_long("key to derive to", logEntryOnDisk),
                           evt_tag_long("number of log entries", *numberOfLogEntries));
                }
              deriveKey(mainKey, logEntryOnDisk, *nextLogEntry);
              *nextLogEntry = logEntryOnDisk;
            }

          GString *line = (GString *)g_ptr_array_index(input, i);
          GString *out = (GString *)g_ptr_array_index(output, i);

          char *ct = &(line->str)[COUNTER_LENGTH + 1];
          gsize outputLength;

          // binBuf = IV + TAG + CT
          guchar *binBuf = g_base64_decode(ct, &outputLength);
          int pt_length = 0;

          // Check whether something weird has happened during conversion
          if (outputLength > IV_LENGTH + AES_BLOCKSIZE)
            {
              guchar pt[outputLength - IV_LENGTH - AES_BLOCKSIZE];
              guchar encKey[KEY_LENGTH];
              deriveEncSubKey(mainKey, encKey);
              pt_length = sLogDecrypt(&binBuf[IV_LENGTH + AES_BLOCKSIZE], outputLength - IV_LENGTH - AES_BLOCKSIZE,
                                      &binBuf[IV_LENGTH],
                                      encKey, binBuf, pt);

              if (pt_length > 0)
                {
                  // Include colon, whitespace, and \0
                  g_string_append_printf(out, "%0*"G_GINT64_MODIFIER"x: %.*s", CTR_LEN_SIMPLE, logEntryOnDisk, pt_length, pt);

                  // Add to table
                  if (!addValueToTable(tab, logEntryOnDisk))
                    {
                      msg_warning(SLOG_WARNING_PREFIX,
                                  evt_tag_str("Reason", "Table entry exists"),
                                  evt_tag_long("Entry: ", logEntryOnDisk));
                      result = FALSE;
                    }

                  // Update BigHMAC
                  gsize outlen = 0;
                  guchar MACKey[KEY_LENGTH];
                  deriveMACSubKey(mainKey, MACKey);
                  //-- now that an inital aggregated MAC exists, do the
                  //   same for first entry as for any other entries!
                  guchar bigBuf[AES_BLOCKSIZE + IV_LENGTH + AES_BLOCKSIZE + pt_length];
                  memcpy(bigBuf, cmac_tag, AES_BLOCKSIZE);
                  memcpy(&bigBuf[AES_BLOCKSIZE], binBuf, IV_LENGTH + AES_BLOCKSIZE + pt_length);
                  if (!cmac(MACKey, bigBuf, AES_BLOCKSIZE + IV_LENGTH + AES_BLOCKSIZE + pt_length, cmac_tag, &outlen, cmac_tag_capacity))
                    {
                      msg_error(SLOG_ERROR_PREFIX,
                                evt_tag_str("Reason", "Bad CMAC"),
                                evt_tag_str("File: ", __FILE__),
                                evt_tag_long("Line: ", __LINE__));
                      result = FALSE;
                    }
                }
            }

          if (pt_length <= 0)
            {
              msg_warning(SLOG_WARNING_PREFIX,
                          evt_tag_str("Reason", "Decryption not successful"),
                          evt_tag_long("entry", logEntryOnDisk));
              result = FALSE;
            }

          g_free(binBuf);

          evolveKey(mainKey);
          (*numberOfLogEntries)++;
          (*nextLogEntry)++;
        }
      else
        {
          msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Cannot read log entry"), evt_tag_long("", *nextLogEntry));
          result = FALSE;
        }

    } // for

  return result;
}

// Perform the final verification step
gboolean finalizeVerify(
  guint64 startingEntry,
  guint64 entriesInFile,
  guchar *aggMAC,
  guchar *cmac_tag,
  GHashTable **tab)
{
  if (tab == NULL || *tab == NULL)
    {
      msg_warning(SLOG_WARNING_PREFIX,
                  evt_tag_str("Reason",
                              "finalizeVerify: hash table already destroyed or not provided"));
      return FALSE;
    }

  int ret = TRUE;

  // Check which entries are missing
  guint64 notRecovered = 0;
  for (guint64 i = startingEntry; i < startingEntry + entriesInFile; i++)
    {
      // Hashtable key
      char key[CTR_LEN_SIMPLE + 1];
      snprintf(key, CTR_LEN_SIMPLE + 1, "%"G_GUINT64_FORMAT, i);
      if (!g_hash_table_contains(*tab, key))
        {
          notRecovered++;
          msg_warning(SLOG_WARNING_PREFIX, evt_tag_str("Reason", "Unable to recover"), evt_tag_long("entry", i));
          ret = FALSE;
        }
    }

  if (notRecovered == 0)
    {
      msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "All entries recovered successfully"));
    }

  int equal = memcmp(aggMAC, cmac_tag, CMAC_LENGTH);

  if (equal != 0)
    {
      // TODO cmac_tag most likely wrong
      msg_warning(SLOG_WARNING_PREFIX, evt_tag_str("Reason", "Aggregated MAC mismatch. Log might be incomplete"));
      ret = FALSE;
    }
  else
    {
      msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Aggregated MAC matches. Log contains all expected log messages."));
    }

  g_hash_table_destroy(*tab);
  *tab = NULL;

  return ret;
}

// Initialize log verification
gboolean initVerify(
  guint64 entriesInFile,
  guchar *mainKey,
  guint64 *nextLogEntry,
  guint64 *startingEntry,
  GPtrArray *input,
  GHashTable **tab)
{
  if (entriesInFile == 0)
    {
      return FALSE;
    }

  // Create hash table
  *tab = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify)g_free, (GDestroyNotify)g_free);

  if (*tab == NULL)
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Cannot create hash table"));
      return FALSE;
    }

  GString *str = (GString *)g_ptr_array_index(input, 0);

  if (str->len > (COUNTER_LENGTH + 1))
    {
      gsize outLen;
      char buf[COUNTER_LENGTH + 1];
      memcpy(buf, str->str, COUNTER_LENGTH);
      buf[COUNTER_LENGTH] = 0;
      guchar *tempInt = g_base64_decode(buf, &outLen);
      if (outLen != sizeof(guint64))
        {
          msg_warning(SLOG_WARNING_PREFIX, evt_tag_str("Reason", "Cannot derive integer value from first input line counter"));
          (*startingEntry) = 0UL;
          g_free(tempInt);
          return FALSE;
        }
      else
        {
          memcpy(startingEntry, tempInt, sizeof(guint64));
          g_free(tempInt);
        }

      if ((*startingEntry) > 0)
        {
          msg_warning(SLOG_WARNING_PREFIX,
                      evt_tag_str("Reason", "Log does not start with index 0"),
                      evt_tag_long("index", (*startingEntry)));
          (*nextLogEntry) = (*startingEntry);
          deriveKey(mainKey, (*nextLogEntry), 0);
          return FALSE;
        }
    }
  else
    {
      msg_warning(SLOG_WARNING_PREFIX, evt_tag_str("Reason", "Problems reading log entry at first line."));
      return FALSE;
    }

  return TRUE;
}


/*
 * Iteratively verify the integrity of an existing log file
 *
 * Return:
 * TRUE on success
 * FALSE on error
 */
gboolean iterativeFileVerify(
  guchar *previousMAC,
  guchar *mainKey,
  char *inputFileName,
  guchar *aggMAC,
  char *outputFileName,
  guint64 entriesInFile,
  int chunkLength,
  guint64 keyNumber)
{

  if (entriesInFile == 0)
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Nothing to verify"));
      return FALSE;
    }

  guchar keyZero[KEY_LENGTH];
  memcpy(keyZero, mainKey, KEY_LENGTH);
  int startedWithZero = 0;

  if (keyNumber != 0)
    {
      msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Verification using a key different from k0."),
               evt_tag_long("Key number: ", keyNumber));
    }
  else
    {
      msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason", "Verification starting with k0. Is this really what you want?"));
      startedWithZero = 1;
    }

  // Create input and output files
  SLogFile *inf = create_file(inputFileName, "r");
  if (inf == NULL)
    {
      return FALSE;
    }
  SLogFile *outf = create_file(outputFileName, "w+");
  if (outf == NULL)
    {
      close_file(inf);
      g_free(inf);
      return FALSE;
    }

  gboolean volatile result = TRUE;

  GPtrArray *inputBuffer = g_ptr_array_new_with_free_func((GDestroyNotify)SLogStringFree);
  GPtrArray *outputBuffer = g_ptr_array_new_with_free_func((GDestroyNotify)SLogStringFree);

  SLOG_SECT_START(inputBuffer)
  // Allocate buffers
  if (inputBuffer == NULL)
    {
      SLogSectForward(__FILE__, __LINE__, SLOG_MEM_ALLOC_ERROR, handleGPtrArrayMemoryError);
    }

  SLOG_SECT_START(outputBuffer)
  if (outputBuffer == NULL)
    {
      SLogSectForward(__FILE__, __LINE__, SLOG_MEM_ALLOC_ERROR, handleGPtrArrayMemoryError);
    }

  SLOG_SECT_START(inf)
  // Open input and output files
  result = open_file(inf);
  if (!result)
    {
      handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, inputBuffer);
      handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, outputBuffer);
      SLogSectForward(__FILE__, __LINE__, inf->state, handleFileError);
    }

  SLOG_SECT_START(outf)
  result = open_file(outf);
  if (!result)
    {
      handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, inputBuffer);
      handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, outputBuffer);
      SLogSectForward(__FILE__, __LINE__, outf->state, handleFileError);
    }

  guchar cmac_tag[CMAC_LENGTH];
  gsize cmac_tag_capacity = G_N_ELEMENTS(cmac_tag);
  memcpy(cmac_tag, previousMAC, CMAC_LENGTH);

  guint64 nextLogEntry = keyNumber;
  guint64 startingEntry = keyNumber;
  guint64 numberOfLogEntries = keyNumber;

  // This is only to avoid updating the aggregated MAC during the first iteration
  //
  if (keyNumber == 0)
    {
      numberOfLogEntries = 1;
    }

  if (chunkLength > entriesInFile)
    {
      chunkLength = entriesInFile;
    }

  // Create the hash table
  GHashTable *tab = g_hash_table_new(g_str_hash, g_str_equal);
  if (tab == NULL)
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Cannot create hash table"));
      result = FALSE;
      // Cannot continue -> Release memory and file resources prematurely
      handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, inputBuffer);
      handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, outputBuffer);
      handleFileError(__FILE__, __LINE__, SLOG_FILE_INCOMPLETE_READ, inf);
      SLogSectForward(__FILE__, __LINE__, outf->state, handleFileError);
    }

  // Process file in chunks
  for (int j = 0; j < (entriesInFile / chunkLength); j++)
    {
      for (guint64 i = 0; i < chunkLength; i++)
        {
          GString *line = getLogEntry(inf);

          if (line != NULL)
            {
              // Add line to buffer
              g_ptr_array_add(inputBuffer, line);
            }
          else
            {
              msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Invalid log entry (= NULL)"));
              result = FALSE;
              // Cannot continue -> Release memory and file resources prematurely
              handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, inputBuffer);
              handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, outputBuffer);
              handleFileError(__FILE__, __LINE__, SLOG_FILE_INCOMPLETE_READ, inf);
              SLogSectForward(__FILE__, __LINE__, outf->state, handleFileError);
            }
        }
      result = iterateBuffer(chunkLength, inputBuffer, &nextLogEntry, mainKey, keyZero, keyNumber, outputBuffer,
                             &numberOfLogEntries, cmac_tag, cmac_tag_capacity, tab);

      // ...and write to file
      for (guint64 i = 0; i < chunkLength; i++)
        {
          GString *line = (GString *)g_ptr_array_index(outputBuffer, i);
          if (line->len != 0)
            {
              result = putLogEntry(outf, line);
              if (!result)
                {
                  handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, inputBuffer);
                  handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, outputBuffer);
                  handleFileError(__FILE__, __LINE__, SLOG_FILE_INCOMPLETE_READ, inf);
                  SLogSectForward(__FILE__, __LINE__, outf->state, handleFileError);
                }
            }
        }
    }

  if ((entriesInFile % chunkLength) > 0)
    {
      for (guint64 i = 0; i < (entriesInFile % chunkLength); i++)
        {
          GString *line = getLogEntry(inf);

          if (line != NULL)
            {
              // Add line to buffer
              g_ptr_array_add(inputBuffer, line);
            }
          else
            {
              msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Invalid log entry (= NULL)"));
              result = FALSE;
              // Cannot continue -> Release memory and file resources prematurely
              handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, inputBuffer);
              handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, outputBuffer);
              handleFileError(__FILE__, __LINE__, SLOG_FILE_INCOMPLETE_READ, outf);
              SLogSectForward(__FILE__, __LINE__, inf->state, handleFileError);
            }
        }

      result = iterateBuffer((entriesInFile % chunkLength), inputBuffer, &nextLogEntry, mainKey, keyZero, keyNumber,
                             outputBuffer, &numberOfLogEntries, cmac_tag, cmac_tag_capacity, tab);

      for (guint64 i = 0; i < (entriesInFile % chunkLength); i++)
        {
          GString *line = (GString *)g_ptr_array_index(outputBuffer, i);
          if (line->len != 0)
            {
              result = putLogEntry(outf, line);
              if (!result)
                {
                  handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, inputBuffer);
                  handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, outputBuffer);
                  handleFileError(__FILE__, __LINE__, SLOG_FILE_INCOMPLETE_READ, inf);
                  SLogSectForward(__FILE__, __LINE__, outf->state, handleFileError);
                }
            }
        }
    }

  if (startedWithZero == 1)
    {
      msg_info(SLOG_INFO_PREFIX, evt_tag_str("Reason",
                                             "We started with key key0. There might be a lot of warnings about missing log entries."));
    }
  if (!finalizeVerify(startingEntry, entriesInFile, aggMAC, cmac_tag, &tab))
    {
      result = FALSE;
    }

  // Close files
  close_file(outf);
  close_file(inf);

  // Release memory resources
  g_ptr_array_free(outputBuffer, TRUE);
  g_ptr_array_free(inputBuffer, TRUE);
  SLOG_SECT_END(outf)
  SLOG_SECT_END(inf)
  SLOG_SECT_END(outputBuffer)
  SLOG_SECT_END(inputBuffer)
  g_free(outf);
  g_free(inf);

  return result;
}

/*
 * Verify the integrity of an existing log file
 *
 * Return:
 * TRUE on success
 * FALSE on error
 */
gboolean fileVerify(guchar *mainKey, char *inputFileName,
                    char *outputFileName, guchar *aggMAC,
                    guint64 entriesInFile, int chunkLength, guchar mac0[CMAC_LENGTH])
{
  gboolean volatile result = TRUE;

  if (entriesInFile == 0)
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Nothing to verify"));
      return 0;
    }



  SLogFile *inf = create_file(inputFileName, "r");
  if (inf == NULL)
    {
      return FALSE;
    }
  SLogFile *outf = create_file(outputFileName, "w+");
  if (outf == NULL)
    {
      close_file(inf);
      g_free(inf);
      return FALSE;
    }

  guchar keyZero[KEY_LENGTH];
  memcpy(keyZero, mainKey, KEY_LENGTH);

  GHashTable *tab = NULL;
  GPtrArray *inputBuffer = g_ptr_array_new_with_free_func((GDestroyNotify)SLogStringFree);
  GPtrArray *outputBuffer = g_ptr_array_new_with_free_func((GDestroyNotify)SLogStringFree);

  SLOG_SECT_START(inputBuffer)
  // Allocate input buffer
  if (inputBuffer == NULL)
    {
      SLogSectForward(__FILE__, __LINE__, SLOG_MEM_ALLOC_ERROR, handleGPtrArrayMemoryError);
    }

  SLOG_SECT_START(outputBuffer)
  // Allocate output buffer
  if (outputBuffer == NULL)
    {
      SLogSectForward(__FILE__, __LINE__, SLOG_MEM_ALLOC_ERROR, handleGPtrArrayMemoryError);
    }

  // Open input and output files
  SLOG_SECT_START(inf)
  result = open_file(inf);
  if (!result)
    {
      handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, inputBuffer);
      handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, outputBuffer);
      SLogSectForward(__FILE__, __LINE__, inf->state, handleFileError);
    }

  SLOG_SECT_START(outf)
  result = open_file(outf);
  if (!result)
    {
      handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, inputBuffer);
      handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, outputBuffer);
      SLogSectForward(__FILE__, __LINE__, outf->state, handleFileError);
    }

  guint64 nextLogEntry = 0UL;
  guint64 startingEntry = 0UL;
  guchar cmac_tag[CMAC_LENGTH];
  gsize cmac_tag_capacity = G_N_ELEMENTS(cmac_tag);
  guint64 numberOfLogEntries = 0UL;

  memcpy(cmac_tag, mac0, CMAC_LENGTH); //-- here the initial MAC file content is needed now

  if (chunkLength > entriesInFile)
    {
      chunkLength = entriesInFile;
    }

  for (guint64 i = 0; i < chunkLength; i++)
    {
      g_ptr_array_add(inputBuffer, g_string_new(NULL));
      result = read_line_from_file(inf,  (GString *)g_ptr_array_index(inputBuffer, i));

      if (!result)
        {
          handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, inputBuffer);
          handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, outputBuffer);
          close_file(outf);
          SLogSectForward(__FILE__, __LINE__, inf->state, handleFileError);
        }

      // Cut last character to remove the trailing new line
      GString *str = (GString *)g_ptr_array_index(inputBuffer, i);
      g_string_truncate(str, (str->len) - 1);
    }

  if (!initVerify(entriesInFile, mainKey, &nextLogEntry, &startingEntry, inputBuffer, &tab))
    {
      if (tab != NULL)
        {
          g_hash_table_destroy(tab);
          tab = NULL;
        }
      //-- TODO cleanup handler instead of goto
      result = FALSE;
      g_print ("\nERROR: Function initVerify fails!\n");
      goto CLEANUP_FILEVERIFY;
    }

  if (!iterateBuffer(chunkLength, inputBuffer, &nextLogEntry, mainKey, keyZero, 0, outputBuffer,
                     &numberOfLogEntries, cmac_tag, cmac_tag_capacity, tab))
    {
      result = FALSE;
    }

  // Write to file
  for (guint64 i = 0; i < chunkLength; i++)
    {
      GString *str = (GString *)g_ptr_array_index(outputBuffer, i);
      if (str->len != 0)
        {
          // Add newline
          g_string_append(str, "\n");
          result = write_to_file(outf, str->str, str->len);
          if (!result)
            {
              handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, inputBuffer);
              handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, outputBuffer);
              close_file(inf);
              SLogSectForward(__FILE__, __LINE__, outf->state, handleFileError);
            }
        }
    }

  // Process file in chunks
  for (int j = 0; j < (entriesInFile / chunkLength) - 1; j++)
    {
      for (guint64 i = 0; i < chunkLength; i++)
        {
          g_ptr_array_add(inputBuffer, g_string_new(NULL));
          result = read_line_from_file(inf,  (GString *)g_ptr_array_index(inputBuffer, i));

          if (!result)
            {
              handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, inputBuffer);
              handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, outputBuffer);
              close_file(outf);
              SLogSectForward(__FILE__, __LINE__, inf->state, handleFileError);
            }

          // Cut last character to remove the trailing new line...
          GString *str = (GString *)g_ptr_array_index(inputBuffer, i);
          g_string_truncate(str, (str->len) - 1);
        }

      if (!iterateBuffer(chunkLength, inputBuffer, &nextLogEntry, mainKey, keyZero, 0, outputBuffer,
                         &numberOfLogEntries, cmac_tag, cmac_tag_capacity, tab))
        {
          result = FALSE;
        }

      // ...and write to file
      for (guint64 i = 0; i < chunkLength; i++)
        {
          GString *str = (GString *)g_ptr_array_index(inputBuffer, i);

          if (str->len != 0)
            {
              // Add newline
              g_string_append(str, "\n");

              result = write_to_file(outf, str->str, str->len);

              if (!result)
                {
                  handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, inputBuffer);
                  handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, outputBuffer);
                  close_file(inf);
                  SLogSectForward(__FILE__, __LINE__, outf->state, handleFileError);
                }
            }
        }
    }

  if ((entriesInFile % chunkLength) > 0)
    {
      for (guint64 i = 0; i < (entriesInFile % chunkLength); i++)
        {
          g_ptr_array_add(inputBuffer, g_string_new(NULL));
          result = read_line_from_file(inf,  (GString *)g_ptr_array_index(inputBuffer, i));

          if (!result)
            {
              handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, inputBuffer);
              handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, outputBuffer);
              close_file(outf);
              SLogSectForward(__FILE__, __LINE__, inf->state, handleFileError);
            }

          // Cut last character to remove the trailing new line
          GString *str = (GString *)g_ptr_array_index(inputBuffer, i);
          g_string_truncate(str, (str->len) - 1);
        }

      if (!iterateBuffer((entriesInFile % chunkLength), inputBuffer, &nextLogEntry, mainKey, keyZero, 0, outputBuffer,
                         &numberOfLogEntries, cmac_tag, cmac_tag_capacity, tab))
        {
          result = FALSE;
        }

      for (guint64 i = 0; i < (entriesInFile % chunkLength); i++)
        {
          GString *str = (GString *)g_ptr_array_index(outputBuffer, i);
          if (str->len != 0)
            {
              // Add newline
              g_string_append(str, "\n");
              result = write_to_file(outf, str->str, str->len);

              if (!result)
                {
                  handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, inputBuffer);
                  handleGPtrArrayMemoryError(__FILE__, __LINE__,  SLOG_MEM_ALLOC_ERROR, outputBuffer);
                  close_file(inf);
                  SLogSectForward(__FILE__, __LINE__, outf->state, handleFileError);
                }
            }
        }
    }
  if (!finalizeVerify(startingEntry, entriesInFile, aggMAC, cmac_tag, &tab))
    {
      result = FALSE;
    }


CLEANUP_FILEVERIFY:

  // Release memory
  //
  g_ptr_array_free(outputBuffer, TRUE);
  g_ptr_array_free(inputBuffer, TRUE);

  // Close files
  close_file(outf);
  close_file(inf);
  SLOG_SECT_END(outf)
  SLOG_SECT_END(inf)
  SLOG_SECT_END(outputBuffer)
  SLOG_SECT_END(inputBuffer)
  g_free(outf);
  g_free(inf);

  return result;
}

// Print usage message and clean up
int slog_usage(GOptionContext *ctx, GOptionGroup *grp, GString *errormsg)
{
  if (errormsg != NULL)
    {
      g_print ("\nERROR: %s\n\n", errormsg->str);
      g_string_free(errormsg, TRUE);
    }

  g_print("%s", g_option_context_get_help(ctx, TRUE, NULL));

  // if (NULL != grp) {
  //    g_print("%s", g_option_context_get_help(ctx, TRUE, grp));
  // } else {
  //    g_print("%s", g_option_context_get_help(ctx, TRUE, NULL));
  // }

  g_option_context_free(ctx);

  return 1;
}

/*
 * Callback function to check whether a command line argument represents a valid file name
 *
 * Return:
 * TRUE on success
 * FALSE on error
 */
gboolean validFileNameArg(const gchar *option_name, const gchar *value, gpointer data, GError **error)
{
  gboolean isValid = FALSE;
  GString *currentOption = g_string_new(option_name);
  GString *currentValue = g_string_new(value);
  GString *longOption = g_string_new(LONG_OPT_INDICATOR);
  GString *shortOption = g_string_new(SHORT_OPT_INDICATOR);

  SLogOptions *opts = (SLogOptions *)data;

  for (SLogOptions *option = opts; option != NULL && option->longname != NULL; option++)
    {
      g_string_append(longOption, option->longname);
      g_string_append_c(shortOption, option->shortname);

      if (g_string_equal(currentOption, longOption) || g_string_equal(currentOption, shortOption))
        {
          if (g_file_test(value, G_FILE_TEST_IS_REGULAR))
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
      *error = g_error_new(G_FILE_ERROR, G_OPTION_ERROR_FAILED, "Invalid path or non existing regular file: %s", value);
    }

  g_string_free(currentOption, TRUE);
  g_string_free(currentValue, FALSE);
  g_string_free(longOption, TRUE);
  g_string_free(shortOption, TRUE);

  return isValid;
}



/*
 * Callback function to check whether a command line argument provides a valid directory with given full file name
 *
 * Return:
 * TRUE on success
 * FALSE on error
 */
gboolean validFileNameArgCheckDirOnly(const gchar *option_name, const gchar *value, gpointer data, GError **error)
{
  gboolean isValid = FALSE;
  GString *currentOption = g_string_new(option_name);
  GString *currentValue = g_string_new(value);
  GString *longOption = g_string_new(LONG_OPT_INDICATOR);
  GString *shortOption = g_string_new(SHORT_OPT_INDICATOR);

  SLogOptions *opts = (SLogOptions *)data;

  for (SLogOptions *option = opts; option != NULL && option->longname != NULL; option++)
    {
      g_string_append(longOption, option->longname);
      g_string_append_c(shortOption, option->shortname);

      if (g_string_equal(currentOption, longOption) || g_string_equal(currentOption, shortOption))
        {
          if (g_file_test(value, G_FILE_TEST_IS_REGULAR))
            {
              option->arg = currentValue->str;
              isValid = TRUE;
              break;
            }

          // If isValid is still FALSE check whether directory exits.
          // This is usefull when a file shall be created, e.g. in case of mac dat
          gchar *dirname = g_path_get_dirname(value);
          if (NULL != dirname)
            {
              if (g_file_test(dirname, G_FILE_TEST_IS_DIR))
                {
                  option->arg = currentValue->str;
                  isValid = TRUE;
                  g_print("directory exits: %s\n", dirname);
                  g_free (dirname);
                  dirname = NULL;
                  break;
                }
            }
          g_free (dirname);

        }

      // Reset for next option argument to check
      g_string_assign(longOption, LONG_OPT_INDICATOR);
      g_string_assign(shortOption, SHORT_OPT_INDICATOR);
    }

  if (!isValid)
    {
      *error = g_error_new(G_FILE_ERROR, G_OPTION_ERROR_FAILED, "Invalid path or non existing regular file: %s", value);
    }

  g_string_free(currentOption, TRUE);
  g_string_free(currentValue, FALSE);
  g_string_free(longOption, TRUE);
  g_string_free(shortOption, TRUE);

  return isValid;
}



// Error handler for file errors
void handleFileError(const char *file, int line, int code, void *object)
{
  SLogFile *f = (SLogFile *)object;
  msg_error(SLOG_ERROR_PREFIX,
            evt_tag_str("File ", file),
            evt_tag_long("Line ", line),
            evt_tag_long("Code ", code),
            evt_tag_str("Reason ", f->message->str)
           );

  if (!close_file(f))
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("Unable to close file stream, File: ", __FILE__),
                evt_tag_long("Line ", __LINE__),
                evt_tag_long("Code ", SLOG_FILE_GENERAL_ERROR),
                evt_tag_str("Reason ", "General file stream error")
               );
    }
}


// Error handler for memory errors
void handleGPtrArrayMemoryError(const char *file, int line, int code, void *object)
{
  GPtrArray *pa = (GPtrArray *)object;
  const gchar *msg1 = "Premature memory release of GPtrArray";
  const gchar *msg2 = "GPtrArray address is NULL";
  const gchar *msg;

  if (pa != NULL)
    {
      g_ptr_array_free(pa, TRUE);
      msg = msg1;
    }
  else
    {
      msg = msg2;
    }
  msg_error(SLOG_ERROR_PREFIX,
            evt_tag_str("File: ", file),
            evt_tag_long("Line: ", line),
            evt_tag_long("Code: ", code),
            evt_tag_str("Reason: ", msg)
           );
}


// Retrieve counter from encrypted log entry
gboolean getCounter(GString *entry, guint64 *logEntryOnDisk)
{
  if (!entry || !logEntryOnDisk || entry->len < COUNTER_LENGTH)
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Cannot derive integer value - not enough data"));
      return FALSE;
    }
  //-- Make a NUL-terminated copy of the base64 chunk
  char *b64 = g_strndup(entry->str, COUNTER_LENGTH);
  gsize out_len;
  guchar *decoded = g_base64_decode(b64, &out_len);
  g_free(b64);
  gboolean ret;

  if (!decoded || out_len != sizeof(guint64))
    {
      msg_error(SLOG_ERROR_PREFIX, evt_tag_str("Reason", "Cannot derive integer value from counter field"));
      ret = FALSE; //-- bad base64 or wrong length
    }
  else
    {
      memcpy(logEntryOnDisk, decoded, sizeof(guint64));
      ret = TRUE;
    }

  g_free(decoded);
  return ret;
}


// Check whether value is contained in table
gboolean tableContainsKey(GHashTable *table, guint64 key)
{
  if (table == NULL)
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("File: ", __FILE__),
                evt_tag_long("Line: ", __LINE__),
                evt_tag_long("Code: ", SLOG_MEM_ALLOC_ERROR),
                evt_tag_str("Reason: ", "Table == NULL")
               );
      return FALSE;
    }

  // Convert to string
  char keystr[CTR_LEN_SIMPLE + 1];
  snprintf(keystr, CTR_LEN_SIMPLE + 1, "%"G_GUINT64_FORMAT, key);

  return g_hash_table_contains(table, keystr);
}

// Add new value to table
gboolean addValueToTable(GHashTable *table, guint64 value)
{
  if (table == NULL)
    {
      msg_error(SLOG_ERROR_PREFIX,
                evt_tag_str("File: ", __FILE__),
                evt_tag_long("Line: ", __LINE__),
                evt_tag_long("Code: ", SLOG_MEM_ALLOC_ERROR),
                evt_tag_str("Reason: ", "Table == NULL")
               );
      return FALSE;
    }

  //-- Create new key to string and use value as key
  char *key = g_strdup_printf("%" G_GUINT64_FORMAT, value);

  //-- Create new value
  guint64 *p_value = g_new(guint64, 1);
  *p_value = value;

  return g_hash_table_insert(table, key, p_value);
}

// Get a single line from a log file
GString *getLogEntry(SLogFile *f)
{
  GString *line = g_string_new(NULL);

  if (!read_line_from_file(f, line))
    {
      g_string_free(line, TRUE);
      line = NULL;
    }
  else
    {
      // Cut last character to remove the trailing new line...
      g_string_truncate(line, line->len - 1);
    }

  return line;
}

// Put a single line into a log file
gboolean putLogEntry(SLogFile *f, GString *line)
{
  if (line == NULL)
    {
      return FALSE;
    }

  if (line->len != 0)
    {
      // Add newline
      g_string_append(line, "\n");
    }

  return write_to_file(f, line->str, line->len);
}

// Clean up routine for GPtrArray
void SLogStringFree(gpointer *arg)
{
  (void) g_string_free((GString *)arg, TRUE);
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
  if (0 == max_octet_len)
    {
      g_string_truncate(gslog, 0);
      return;
    }
  gsize max_text_len = max_octet_len - 1; //-- reserve one byte for '\n'
  gsize new_len = max_text_len;
  //--Walk backwards to find a valid UTF-8 character / Symbol (Emoij) boundary.
  while (new_len > 0 && (gslog->str[new_len] & 0xC0) == 0x80)
    {
      new_len--;
    }
  g_string_truncate(gslog, new_len);
  g_string_append_c(gslog, '\n');
}

