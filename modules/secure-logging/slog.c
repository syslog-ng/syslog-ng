/*
 * Copyright (c) 2019 Airbus Commercial Aircraft
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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <openssl/cmac.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include "messages.h"

#include "slog.h"

// Argument indicators for command line utilities
#define LONG_OPT_INDICATOR "--"
#define SHORT_OPT_INDICATOR "-"

// This initialization only works with GCC.
static unsigned char KEYPATTERN[AES_BLOCKSIZE] = { [0 ... (AES_BLOCKSIZE-1) ] = IPAD };
static unsigned char MACPATTERN[AES_BLOCKSIZE] = { [0 ... (AES_BLOCKSIZE-1) ] = OPAD };
static unsigned char GAMMA[AES_BLOCKSIZE] = { [0 ... (AES_BLOCKSIZE-1) ] =  EPAD};

/*
 * Conditional msg_error output.
 *
 * 1. Parameter: error variable (input)
 * 2. Parameter: main error string to output (input)
 */
void cond_msg_error(GError *myError, char *errorMsg)
{

  if (myError==NULL)
    {
      msg_error(errorMsg);
    }
  else
    {
      msg_error(errorMsg, evt_tag_str("error", myError->message));
    }

}

/*
 * Create specific sub-keys for encryption and CMAC generation from key.
 *
 * 1. Parameter: (main) key (input)
 * 2. Parameter: encryption key (output)
 * 3. Parameter: (C)MAC key (output)
 *
 * Note: encKey and MACKey must have space to hold KEY_LENGTH many bytes.
 */
void deriveSubKeys(unsigned char *mainKey, unsigned char *encKey, unsigned char *MACKey)
{
  deriveEncSubKey(mainKey, encKey);
  deriveMACSubKey(mainKey, MACKey);
}

void deriveEncSubKey(unsigned char *mainKey, unsigned char *encKey)
{
  PRF(mainKey, KEYPATTERN, sizeof(KEYPATTERN), encKey, KEY_LENGTH);
}

void deriveMACSubKey(unsigned char *mainKey, unsigned char *MACKey)
{
  PRF(mainKey, MACPATTERN, sizeof(MACPATTERN), MACKey, KEY_LENGTH);
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
int sLogEncrypt(unsigned char *plaintext, int plaintext_len,
                unsigned char *key, unsigned char *iv,
                unsigned char *ciphertext, unsigned char *tag)
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
  if(!(ctx = EVP_CIPHER_CTX_new()))
    {
      msg_error("[SLOG] ERROR: Unable to initialize OpenSSL context");
      return 0;
    }

  /* Initialise the encryption operation. */
  if(1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL))
    {
      msg_error("[SLOG] ERROR: Unable to initialize OpenSSL context");
      return 0;
    }

  if (IV_LENGTH!=12)
    {
      /* Set IV length if default 12 bytes (96 bits) is not appropriate */
      if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_LENGTH, NULL))
        {
          msg_error("[SLOG] ERROR: Unable to set IV length");
          return 0;
        }
    }

  /* Initialise key and IV */
  if(1 != EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv))
    {
      msg_error("[SLOG] ERROR: Unable to initialize encryption key and IV");
      return 0;
    }

  /* Provide the message to be encrypted, and obtain the encrypted output.
   * EVP_EncryptUpdate can be called multiple times if necessary
   */
  if(1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len))
    {
      msg_error("[SLOG] ERROR: Unable to encrypt data");
      return 0;
    }

  ciphertext_len = len;

  /* Finalise the encryption. Normally ciphertext bytes may be written at
   * this stage, but this does not occur in GCM mode
   */
  if(1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len))
    {
      msg_error("[SLOG] ERROR: Unable to complete encryption of data");
      return 0;
    }

  ciphertext_len += len;

  /* Get the tag */
  if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, AES_BLOCKSIZE, tag))
    {
      msg_error("[SLOG] ERROR: Unable to acquire encryption tag");
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
int sLogDecrypt(unsigned char *ciphertext, int ciphertext_len, unsigned char *tag, unsigned char *key,
                unsigned char *iv,
                unsigned char *plaintext)
{
  EVP_CIPHER_CTX *ctx;
  int len;
  int plaintext_len;
  int ret;

  /* Create and initialise the context */
  if(!(ctx = EVP_CIPHER_CTX_new()))
    {
      msg_error("[SLOG] ERROR: Unable to initialize OpenSSL context");
      return 0;
    }

  /* Initialise the decryption operation. */
  if(!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL))
    {
      msg_error("[SLOG] ERROR: Unable initiate decryption operation");
      return 0;
    }

  if(IV_LENGTH!=12)
    {
      /* Set IV length. Not necessary if this is 12 bytes (96 bits) */
      if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_LENGTH, NULL))
        {
          msg_error("[SLOG] ERROR: Unable set IV length");
          return 0;
        }
    }

  /* Initialise key and IV */
  if(!EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv))
    {
      msg_error("[SLOG] ERROR: Unable to initialize key and IV");
      return 0;
    }

  /* Provide the message to be decrypted, and obtain the plaintext output.
   * EVP_DecryptUpdate can be called multiple times if necessary
   */
  if(!EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len))
    {
      msg_error("Unable to decrypt");
      return 0;
    }

  plaintext_len = len;

  /* Set expected tag value. Works in OpenSSL 1.0.1d and later */
  if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, AES_BLOCKSIZE, tag))
    {
      msg_error("[SLOG] ERROR: Unable set tag value");
      return 0;
    }

  /* Finalise the decryption. A positive return value indicates success,
   * anything else is a failure - the plaintext is not trustworthy.
   */
  ret = EVP_DecryptFinal_ex(ctx, plaintext + len, &len);

  /* Clean up */
  EVP_CIPHER_CTX_free(ctx);
  if(ret > 0)
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
*/
void sLogEntry(guint64 numberOfLogEntries, GString *text, unsigned char *mainKey, unsigned char *inputBigMac,
               GString *output, unsigned char *outputBigMac)
{

  unsigned char encKey[KEY_LENGTH];
  unsigned char MACKey[KEY_LENGTH];
  deriveSubKeys(mainKey, encKey, MACKey);

  // Compute current log entry number
  gchar *counterString = convertToBase64((unsigned char *)&numberOfLogEntries, sizeof(numberOfLogEntries));

  int slen = (int) text->len;

  // This buffer holds everything: AggregatedMAC, IV, Tag, and CText
  // Binary data cannot be larger than its base64 encoding
  unsigned char bigBuf[AES_BLOCKSIZE+IV_LENGTH+AES_BLOCKSIZE+slen];

  // This is where are ciphertext related data starts
  unsigned char *ctBuf = &bigBuf[AES_BLOCKSIZE];
  unsigned char *iv = ctBuf;
  unsigned char *tag = &bigBuf[AES_BLOCKSIZE+IV_LENGTH];
  unsigned char *ciphertext = &bigBuf[AES_BLOCKSIZE+IV_LENGTH+AES_BLOCKSIZE];

  // Generate random nonce
  if (RAND_bytes(iv, IV_LENGTH)==1)
    {

      // Encrypt log data
      int ct_length = sLogEncrypt((guchar *)text->str, slen, encKey, iv, ciphertext, tag);
      if(ct_length <= 0)
        {
          msg_error("[SLOG] ERROR: Unable to correctly encrypt log message");

          g_string_printf(output, "%*.*s:%s: %s", COUNTER_LENGTH, COUNTER_LENGTH, counterString,
                          "[SLOG] ERROR: Unable to correctly encrypt the following log message:", text->str);
          g_free(counterString);

          return;
        }

      // Write current log entry number
      g_string_printf (output, "%*.*s:", COUNTER_LENGTH, COUNTER_LENGTH, counterString);
      g_free(counterString);


      // Write IV, tag, and ciphertext at once
      gchar *encodedCtBuf = convertToBase64(ctBuf, IV_LENGTH+AES_BLOCKSIZE+ct_length);
      g_string_append(output, encodedCtBuf);
      g_free(encodedCtBuf);

      // Compute aggregated MAC
      // Not the first aggregated MAC
      if (numberOfLogEntries>0)
        {
          memcpy(bigBuf, inputBigMac, AES_BLOCKSIZE);

          gsize outlen;
          cmac(MACKey, bigBuf, AES_BLOCKSIZE+IV_LENGTH+AES_BLOCKSIZE+ct_length, outputBigMac, &outlen );
        }
      else   // First aggregated MAC
        {
          gsize outlen = 0;

          cmac(MACKey, &bigBuf[AES_BLOCKSIZE], IV_LENGTH+AES_BLOCKSIZE+ct_length, outputBigMac, &outlen);
        }
    }
  else
    {
      // We did not get enough random bytes
      msg_error("[SLOG] ERROR: Could not obtain enough random bytes");

      g_string_printf(output, "%*.*s:%s: %s", COUNTER_LENGTH, COUNTER_LENGTH, counterString,
                      "[SLOG] ERROR: Could not obtain enough random bytes for the following log message:", text->str);
      g_free(counterString);

      return;
    }
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
void deriveKey(unsigned char *dst, guint64 index, guint64 currentKey)
{
  for (guint64 i = currentKey; i<index; i++)
    {
      evolveKey(dst);
    }
}

guchar *convertToBin(char *input, gsize *outLen)
{
  return g_base64_decode ((const gchar *) input, outLen);
}

gchar *convertToBase64(unsigned char *input, gsize len)
{
  return  g_base64_encode ((const guchar *) input, len);
}

/*
 * Compute AES256 CMAC of input
 *
 * 1. Parameter: Pointer to key (input)
 * 2. Parameter: Pointer to input (input)
 * 3. Parameter: Input length (input)
 * 4. Parameter: Pointer to output (output)
 * 5. Parameter: Length of output (output)
 *
 * Note: caller must take care of memory management.
 *
 * If Parameter 5 == 0, there was an error.
 *
 */
void cmac(unsigned char *key, const void *input, gsize length, unsigned char *out, gsize *outlen)
{
  CMAC_CTX *ctx = CMAC_CTX_new();

  CMAC_Init(ctx, key, KEY_LENGTH, EVP_aes_256_cbc(), NULL);
  CMAC_Update(ctx, input, length);

  size_t outsize;
  CMAC_Final(ctx, out, &outsize);
  *outlen = outsize;
  CMAC_CTX_free(ctx);
}


/*
 *  Evolve key
 *
 * 1. Parameter: Pointer to key (input/output)
 *
 */

void evolveKey(unsigned char *key)
{

  unsigned char buf[KEY_LENGTH];
  PRF(key, GAMMA, sizeof(GAMMA), buf, KEY_LENGTH);
  memcpy(key, buf, KEY_LENGTH);
}

/*
 * AES-CMAC based pseudo-random function (with variable input length and output length)
 *
 * 1. Parameter: Pointer to key (input)
 * 2. Parameter: Pointer to input (input)
 * 3. Parameter: Length of input (input)
 * 4. Parameter: Pointer to output (output)
 * 5. Parameter: Required output length (input)
 *
 * Note: For security, outputLength must be less than 255 * AES_BLOCKSIZE.
 *
 */
void PRF(unsigned char *key, unsigned char *originalInput, guint64 inputLength, unsigned char *output,
         guint64 outputLength)
{

  unsigned char input[inputLength];
  memcpy(input, originalInput, inputLength);

  // Make sure that temporary buffer can hold at least outputLength bytes, rounded up to a multiple of AES_BLOCKSIZE
  unsigned char buf[outputLength+AES_BLOCKSIZE];
  // Prepare plaintext
  for (int i=0; i<outputLength/AES_BLOCKSIZE; i++)
    {
      gsize outlen;
      cmac(key, input, AES_BLOCKSIZE, &buf[i*AES_BLOCKSIZE], &outlen);
      input[inputLength-1]++;
    }

  if (outputLength % AES_BLOCKSIZE!=0)
    {
      int index = outputLength/AES_BLOCKSIZE;
      gsize outlen;
      cmac(key, input, AES_BLOCKSIZE, &buf[(index)*AES_BLOCKSIZE], &outlen);
    }

  memcpy(output, buf, outputLength);
}


/*
 * Generate a master key
 *
 * This unique master key requires 32 bytes of storage.
 * The caller has to allocate this memory.
 *
 * Return:
 * 1 on success
 * 0 on error
 */
int generateMasterKey(guchar *masterkey)
{
  return RAND_bytes(masterkey, KEY_LENGTH);
}

/*
 * Generate a host key based on a previously created master key
 *
 * 1. Parameter: master key
 * 2. Parameter: Host MAC address
 * 3. Parameter: Host S/N
 *
 * The specific unique host key k_0 is k_0 = H(master key|| MAC address || S/N)
 * and requires 48 bytes of storage. Additional 8 bytes need to be allocated to store
 * the serial number of the host key. The caller has to allocate this memory.
 *
 * Return:
 * 1 on success
 * 0 on error
 */
int deriveHostKey(guchar *masterkey, gchar *macAddr, gchar *serial, guchar *hostkey)
{
  EVP_MD_CTX *ctx;

  if((ctx = EVP_MD_CTX_create()) == NULL)
    return 0;

  if(1 != EVP_DigestInit_ex(ctx, EVP_sha256(), NULL))
    return 0;

  if(1 != EVP_DigestUpdate(ctx, masterkey, KEY_LENGTH))
    return 0;

  if(1 != EVP_DigestUpdate(ctx, macAddr, strlen(macAddr)))
    return 0;

  if(1 != EVP_DigestUpdate(ctx, serial, strlen(serial)))
    return 0;

  if(KEY_LENGTH != SHA256_DIGEST_LENGTH)
    {
      msg_error("[SLOG] ERROR: Error in updating digest");
      g_assert_not_reached();
      return 0;
    }

  guint digest_len = KEY_LENGTH;

  if(1 != EVP_DigestFinal_ex(ctx, hostkey, &digest_len))
    return 0;

  EVP_MD_CTX_destroy(ctx);

  return 1;
}

/*
 *  Write whole log MAC to file
 *
 * Return:
 * 1 on success
 * 0 on error
 */
int writeBigMAC(gchar *filename, char *outputBuffer)
{
  GError *error = NULL;

  GIOChannel *macfile = g_io_channel_new_file(filename, "w+", &error);
  if(!macfile)
    {
      msg_error("[SLOG] ERROR: Unable open MAC file",
                evt_tag_str("base_dir", filename));
      cond_msg_error(error, "Additional Information");

      g_clear_error(&error);

      return 0;
    }

  GIOStatus status = g_io_channel_set_encoding(macfile, NULL, &error);
  if(status != G_IO_STATUS_NORMAL)
    {
      msg_error("[SLOG] ERROR: Unable to set encoding for MAC data",
                evt_tag_str("File", filename));
      cond_msg_error(error, "Additional information");

      g_clear_error(&error);

      g_io_channel_shutdown(macfile, TRUE, &error);
      g_io_channel_unref(macfile);

      g_clear_error(&error);

      return 0;
    }

  gsize outlen = 0;
  status = g_io_channel_write_chars(macfile, outputBuffer, CMAC_LENGTH, &outlen, &error);
  if(status != G_IO_STATUS_NORMAL)
    {
      msg_error("[SLOG] ERROR: Unable to write big MAC data",
                evt_tag_str("File", filename));
      cond_msg_error(error, "Additional information");

      g_clear_error(&error);

      g_io_channel_shutdown(macfile, TRUE, &error);
      g_io_channel_unref(macfile);

      g_clear_error(&error);

      return 0;
    }

  // Compute aggregated MAC
  gchar outputmacdata[CMAC_LENGTH];
  unsigned char keyBuffer[KEY_LENGTH];
  bzero(keyBuffer, KEY_LENGTH);
  unsigned char zeroBuffer[CMAC_LENGTH];
  bzero(zeroBuffer, CMAC_LENGTH);
  memcpy(keyBuffer, outputBuffer, MIN(CMAC_LENGTH, KEY_LENGTH));
  cmac(keyBuffer, zeroBuffer, CMAC_LENGTH, (guchar *)outputmacdata, &outlen);

  status = g_io_channel_write_chars(macfile, outputmacdata, CMAC_LENGTH, &outlen, &error);

  if(status != G_IO_STATUS_NORMAL)
    {
      msg_error("[SLOG] ERROR: Unable to write aggregated MAC",
                evt_tag_str("File", filename));
      cond_msg_error(error, "Additional information");

      g_clear_error(&error);

      g_io_channel_shutdown(macfile, TRUE, &error);
      g_io_channel_unref(macfile);

      g_clear_error(&error);

      return 0;
    }
  status = g_io_channel_shutdown(macfile, TRUE, &error);
  g_io_channel_unref(macfile);

  if(status != G_IO_STATUS_NORMAL)
    {
      cond_msg_error(error, "[SLOG] ERROR: Cannot close aggregated MAC");

      g_clear_error(&error);
    }

  return 1;
}

/*
 * Read whole log MAC from file
 *
 * Return:
 * 1 on success
 * 0 on error
 */
int readBigMAC(gchar *filename, char *outputBuffer)
{
  GError *myError = NULL;

  GIOChannel *macfile = g_io_channel_new_file(filename, "r", &myError);

  if(!macfile)
    {
      // MAC file does not exist -> New MAC file will be created
      g_clear_error(&myError);

      return 0;
    }

  GIOStatus status = g_io_channel_set_encoding(macfile, NULL, &myError);
  if (status != G_IO_STATUS_NORMAL)
    {
      cond_msg_error(myError, "[SLOG] ERROR: Cannot set encoding of MAC file");
      g_clear_error(&myError);

      g_io_channel_shutdown(macfile, TRUE, &myError);
      g_io_channel_unref(macfile);

      g_clear_error(&myError);

      return 0;
    }

  gchar macdata[2*CMAC_LENGTH];
  gsize mac_bytes_read = 0;

  status = g_io_channel_read_chars(macfile, macdata, 2*CMAC_LENGTH, &mac_bytes_read, &myError);
  if(status != G_IO_STATUS_NORMAL)
    {
      cond_msg_error(myError, "[SLOG] ERROR: Cannot read MAC file");

      g_clear_error(&myError);

      g_io_channel_shutdown(macfile, TRUE, &myError);
      g_io_channel_unref(macfile);

      g_clear_error(&myError);

      return 0;
    }

  status = g_io_channel_shutdown(macfile, TRUE, &myError);
  g_io_channel_unref(macfile);

  if(status != G_IO_STATUS_NORMAL)
    {
      msg_error("[SLOG] ERROR: Cannot close MAC file");

      g_clear_error(&myError);

      return 0;
    }

  if (mac_bytes_read!=2*CMAC_LENGTH)
    {
      msg_error("[SLOG] ERROR: $(slog) parsing failed, invalid size of MAC file");
      return 0;
    }

  gsize outlen = 0;
  unsigned char keyBuffer[KEY_LENGTH];
  bzero(keyBuffer, KEY_LENGTH);
  unsigned char zeroBuffer[CMAC_LENGTH];
  bzero(zeroBuffer, CMAC_LENGTH);
  memcpy(keyBuffer, macdata, MIN(CMAC_LENGTH, KEY_LENGTH));

  unsigned char testOutput[CMAC_LENGTH];
  cmac(keyBuffer, zeroBuffer, CMAC_LENGTH, testOutput, &outlen);

  if (0 != memcmp(testOutput, &macdata[CMAC_LENGTH], CMAC_LENGTH))
    {
      msg_warning("[SLOG] ERROR: MAC computation invalid");
      return 0;
    }
  else
    {
      msg_info("[SLOG] INFO: MAC successfully loaded");
    }

  memcpy(outputBuffer, macdata, CMAC_LENGTH);

  return 1;
}

/*
 *  Read key from file
 *
 * Return:
 * 1 on success
 * 0 on error
 */
int readKey(char *destKey, guint64 *destCounter, gchar *keypath)
{

  GError *myError = NULL;

  GIOChannel *keyfile = g_io_channel_new_file(keypath, "r", &myError);

  if (!keyfile)
    {
      cond_msg_error(myError, "[SLOG] ERROR: Key file not found");
      g_clear_error(&myError);

      return 0;
    }

  GIOStatus status = g_io_channel_set_encoding(keyfile, NULL, &myError);

  if(status != G_IO_STATUS_NORMAL)
    {
      cond_msg_error(myError, "[SLOG] ERROR: Unable to set encoding for key file");
      g_clear_error(&myError);

      g_io_channel_shutdown(keyfile, TRUE, &myError);
      g_io_channel_unref(keyfile);

      g_clear_error(&myError);


      return 0;
    }

  // Key file contains
  // 1. the number of log entries already logged (and therewith the index of the key); this is at the end of the key file
  // 2. the actual 32 byte key, this is at the beginning of the key file
  // 3. a 16 byte CMAC of the two previous data, this is stored after the key

  gchar keydata[KEY_LENGTH + CMAC_LENGTH];
  gsize key_bytes_read = 0;

  status = g_io_channel_read_chars(keyfile, keydata, KEY_LENGTH + CMAC_LENGTH, &key_bytes_read, &myError);

  if (status != G_IO_STATUS_NORMAL)
    {
      cond_msg_error(myError, "[SLOG] ERROR: Cannot read from key file");

      g_clear_error(&myError);

      g_io_channel_shutdown(keyfile, TRUE, &myError);
      g_io_channel_unref(keyfile);

      g_clear_error(&myError);

      return 0;

    }

  if (key_bytes_read!=KEY_LENGTH+CMAC_LENGTH)
    {
      msg_error("[SLOG] ERROR: Invalid key file. Missing CMAC");

      status = g_io_channel_shutdown(keyfile, TRUE, &myError);
      g_io_channel_unref(keyfile);

      g_clear_error(&myError);

      return 0;
    }

  guint64 littleEndianCounter;

  status = g_io_channel_read_chars(keyfile, (gchar *) &littleEndianCounter, sizeof(littleEndianCounter), &key_bytes_read,
                                   &myError);
  if (status != G_IO_STATUS_NORMAL)
    {
      cond_msg_error(myError, "[SLOG] ERROR: Cannot read counter from key file");

      g_clear_error(&myError);

      g_io_channel_shutdown(keyfile, TRUE, &myError);
      g_io_channel_unref(keyfile);

      g_clear_error(&myError);

      return 0;
    }

  status = g_io_channel_shutdown(keyfile, TRUE, &myError);
  g_io_channel_unref(keyfile);

  g_clear_error(&myError);

  if (key_bytes_read!=sizeof(littleEndianCounter))
    {
      msg_error("[SLOG] ERROR: $(slog) parsing failed, key file invalid while reading counter");
      return 0;
    }

  gsize outlen=0;
  unsigned char testOutput[CMAC_LENGTH];

  cmac((guchar *)keydata, &(littleEndianCounter), sizeof(littleEndianCounter), testOutput, &outlen);

  if (0!=memcmp(testOutput, &keydata[KEY_LENGTH], CMAC_LENGTH))
    {
      msg_warning("[SLOG] ERROR: Host key corrupted. CMAC in key file not matching");
      return 0;
    }

  memcpy(destKey, keydata, KEY_LENGTH);
  *destCounter = GUINT64_FROM_LE(littleEndianCounter);

  return 1;
}

/*
 * Write key to file
 *
 * Return:
 * 1 on success
 * 0 on error
 */
int writeKey(char *key, guint64 counter, gchar *keypath)
{
  GError *error = NULL;
  GIOChannel *keyfile = g_io_channel_new_file(keypath, "w+", &error);

  if(!keyfile)
    {
      cond_msg_error(error, "[SLOG] ERROR: Cannot open key file");

      g_clear_error(&error);

      return 0;
    }

  GIOStatus status = g_io_channel_set_encoding(keyfile, NULL, &error);
  if(status != G_IO_STATUS_NORMAL)
    {
      cond_msg_error(error, "[SLOG] ERROR: Unable to set encoding for key file");

      g_clear_error(&error);

      g_io_channel_shutdown(keyfile, TRUE, &error);
      g_io_channel_unref(keyfile);

      g_clear_error(&error);

      return 0;
    }

  guint64 outlen = 0;
  // Write key
  status = g_io_channel_write_chars(keyfile, key, KEY_LENGTH, &outlen, &error);
  if(status != G_IO_STATUS_NORMAL)
    {
      cond_msg_error(error, "[SLOG] ERROR: Unable to write updated key");

      g_clear_error(&error);

      g_io_channel_shutdown(keyfile, TRUE, &error);
      g_io_channel_unref(keyfile);

      g_clear_error(&error);

      return 0;
    }

  guint64 littleEndianCounter = GINT64_TO_LE(counter);
  gchar outputmacdata[CMAC_LENGTH];
  cmac((guchar *)key, &littleEndianCounter, sizeof(littleEndianCounter), (guchar *)outputmacdata, &outlen);

  // Write CMAC
  status = g_io_channel_write_chars(keyfile, outputmacdata, CMAC_LENGTH, &outlen, &error);
  if(status != G_IO_STATUS_NORMAL)
    {
      cond_msg_error(error, "[SLOG] ERROR: Unable to write key CMAC");

      g_clear_error(&error);

      g_io_channel_shutdown(keyfile, TRUE, &error);
      g_io_channel_unref(keyfile);

      g_clear_error(&error);

      return 0;
    }

  // Write counter
  status = g_io_channel_write_chars(keyfile, (gchar *) &littleEndianCounter, sizeof(littleEndianCounter), &outlen,
                                    &error);
  if(status != G_IO_STATUS_NORMAL)
    {
      cond_msg_error(error, "[SLOG] ERROR: Unable to write key counter");

      g_clear_error(&error);

      g_io_channel_shutdown(keyfile, TRUE, &error);
      g_io_channel_unref(keyfile);

      g_clear_error(&error);

      return 0;
    }

  status = g_io_channel_shutdown(keyfile, TRUE, &error);
  g_io_channel_unref(keyfile);

  if (status != G_IO_STATUS_NORMAL)
    {
      cond_msg_error(error, "[SLOG] ERROR: Cannot close key file");

      g_clear_error(&error);

      return 0;
    }

  return 1;

}

int iterateBuffer(guint64 entriesInBuffer, GString **input, guint64 *nextLogEntry, unsigned char *mainKey,
                  unsigned char *keyZero, guint keyNumber, GString **output, guint64 *numberOfLogEntries, unsigned char *cmac_tag,
                  GHashTable *tab)
{

  int ret = 1;
  for (guint64 i=0; i<entriesInBuffer; i++)
    {

      output[i] = g_string_new(NULL);

      guint64 len = input[i]->len;
      if (len > (COUNTER_LENGTH + 1))
        {
          // Interpret the first COUNTER_LENGTH+1 characters
          char ctrbuf[COUNTER_LENGTH+1];
          memcpy(ctrbuf, input[i]->str, COUNTER_LENGTH);
          ctrbuf[COUNTER_LENGTH] = 0;

          gsize outLen;
          guchar *tmp = convertToBin(ctrbuf, &outLen);

          guint64 logEntryOnDisk;
          if (outLen!=sizeof(guint64))
            {
              msg_error("[SLOG] ERROR: Cannot derive integer value from counter field", evt_tag_long("Log entry number",
                        *nextLogEntry));
              logEntryOnDisk = *nextLogEntry;
              g_free(tmp);
            }
          else
            {
              memcpy(&logEntryOnDisk, tmp, sizeof(logEntryOnDisk));
              g_free(tmp);
            }

          len = len - (COUNTER_LENGTH+1);

          if (logEntryOnDisk != *nextLogEntry)
            {
              if (tab != NULL)
                {
                  char key[CTR_LEN_SIMPLE+1];
                  snprintf(key, CTR_LEN_SIMPLE+1, "%"G_GUINT64_FORMAT, logEntryOnDisk);
                  if(g_hash_table_contains(tab, key) == TRUE)
                    {
                      msg_error("[SLOG] ERROR: Duplicate entry detected", evt_tag_long("entry", logEntryOnDisk));
                      ret = 0;
                    }
                }
              if (logEntryOnDisk<(*nextLogEntry))
                {
                  if (logEntryOnDisk<keyNumber)
                    {
                      msg_error("[SLOG] ERROR: Log claims to be past entry from past archive. We cannot rewind back to this key without key0. This is going to fail.",
                                evt_tag_long("entry", logEntryOnDisk));
                      ret = 0;
                    }
                  else
                    {
                      msg_error("[SLOG] ERROR: Log claims to be past entry. We rewind from first known key, this might take some time",
                                evt_tag_long("entry", logEntryOnDisk));
                      // Rewind key to k0
                      memcpy(mainKey, keyZero, KEY_LENGTH);
                      deriveKey(mainKey, logEntryOnDisk, keyNumber);
                      *nextLogEntry = logEntryOnDisk;
                      ret = 0;
                    }
                }
              if (logEntryOnDisk-(*nextLogEntry)>1000000)
                {
                  msg_info("[SLOG] INFO: Deriving key for distant future. This might take some time.",
                           evt_tag_long("next log entry should be", *nextLogEntry), evt_tag_long("key to derive to", logEntryOnDisk),
                           evt_tag_long("number of log entries", *numberOfLogEntries));
                }
              deriveKey(mainKey, logEntryOnDisk, *nextLogEntry);
              *nextLogEntry = logEntryOnDisk;
            }

          GString *line = input[i];

          char *ct = &(line->str)[COUNTER_LENGTH+1];
          gsize outputLength;

          // binBuf = IV + TAG + CT
          guchar *binBuf = convertToBin(ct, &outputLength);
          int pt_length = 0;

          // Check whether something weird has happened during conversion
          if (outputLength>IV_LENGTH+AES_BLOCKSIZE)
            {
              unsigned char pt[outputLength - IV_LENGTH - AES_BLOCKSIZE];

              unsigned char encKey[KEY_LENGTH];
              deriveEncSubKey(mainKey, encKey);

              pt_length = sLogDecrypt(&binBuf[IV_LENGTH+AES_BLOCKSIZE], outputLength - IV_LENGTH - AES_BLOCKSIZE, &binBuf[IV_LENGTH],
                                      encKey, binBuf, pt);

              if (pt_length>0)
                {
                  // Include colon, whitespace, and \0
                  g_string_append_printf(output[i], "%0*"G_GINT64_MODIFIER"x: %.*s", CTR_LEN_SIMPLE, logEntryOnDisk, pt_length, pt);

                  if (tab != NULL)
                    {
                      char *key = g_new0(char, CTR_LEN_SIMPLE+1);
                      snprintf(key, CTR_LEN_SIMPLE+1, "%"G_GUINT64_FORMAT, logEntryOnDisk);

                      if (g_hash_table_insert(tab, key, (gpointer)logEntryOnDisk) == FALSE)
                        {
                          msg_warning("[SLOG] WARNING: Unable to process hash table while entering decrypted log entry", evt_tag_long("entry",
                                      logEntryOnDisk));
                          ret = 0;
                        }
                    }

                  // Update BigHMAC
                  if ((*numberOfLogEntries) == 0UL)   // First aggregated MAC
                    {
                      gsize outlen = 0;

                      unsigned char MACKey[KEY_LENGTH];
                      deriveMACSubKey(mainKey, MACKey);

                      cmac(MACKey, binBuf, IV_LENGTH+AES_BLOCKSIZE+pt_length, cmac_tag, &outlen );
                    }
                  else
                    {
                      // numberOfEntries > 0
                      gsize outlen;
                      unsigned char bigBuf[AES_BLOCKSIZE+IV_LENGTH+AES_BLOCKSIZE+pt_length];
                      memcpy(bigBuf, cmac_tag, AES_BLOCKSIZE);
                      memcpy(&bigBuf[AES_BLOCKSIZE], binBuf, IV_LENGTH+AES_BLOCKSIZE+pt_length);

                      unsigned char MACKey[KEY_LENGTH];
                      deriveMACSubKey(mainKey, MACKey);

                      cmac(MACKey, bigBuf, AES_BLOCKSIZE+IV_LENGTH+AES_BLOCKSIZE+pt_length, cmac_tag, &outlen );
                    }
                }
            }

          if (pt_length<=0)
            {
              msg_warning("[SLOG] WARNING: Decryption not successful",
                          evt_tag_long("entry", logEntryOnDisk));
              ret = 0;
            }

          g_free(binBuf);

          evolveKey(mainKey);
          (*numberOfLogEntries)++;
          (*nextLogEntry)++;

        }
      else
        {
          msg_error("[SLOG] ERROR: Cannot read log entry", evt_tag_long("", *nextLogEntry));
          ret = 0;
        }

    } // for

  return ret;
}

// Perform the final verification step
int finalizeVerify(guint64 startingEntry, guint64 entriesInFile, unsigned char *bigMac, unsigned char *cmac_tag,
                   GHashTable *tab)
{

  int ret = 1;

  // Check which entries are missing
  guint64 notRecovered = 0;
  for (guint64 i = startingEntry; i < startingEntry + entriesInFile; i++)
    {
      if (tab != NULL)
        {
          // Hashtable key
          char key[CTR_LEN_SIMPLE+1];
          snprintf(key, CTR_LEN_SIMPLE+1, "%"G_GUINT64_FORMAT, i);

          if(g_hash_table_contains(tab, key) == FALSE)
            {
              notRecovered++;
              msg_warning("[SLOG] WARNING: Unable to recover", evt_tag_long("entry", i));
              ret = 0;
            }
        }
    }

  if ((notRecovered == 0) && (tab != NULL))
    {
      msg_info("[SLOG] INFO: All entries recovered successfully");
    }

  int equal = memcmp(bigMac, cmac_tag, CMAC_LENGTH);

  if (equal != 0)
    {
      msg_warning("[SLOG] WARNING: Aggregated MAC mismatch. Log might be incomplete");
      ret = 0;
    }
  else
    {
      msg_info("[SLOG] Aggregated MAC matches. Log contains all expected log messages.");
    }

  g_hash_table_unref(tab);

  return ret;
}

// Initialize log verification
int initVerify(guint64 entriesInFile, unsigned char *mainKey, guint64 *nextLogEntry, guint64 *startingEntry,
               GString **input, GHashTable **tab)
{
  // Create hash table
  *tab = g_hash_table_new(g_str_hash, g_str_equal);
  if (*tab == NULL)
    {
      msg_error("[SLOG] ERROR: Cannot create hash table");
      return 0;
    }

  if (input[0]->len>(COUNTER_LENGTH+1))
    {
      gsize outLen;
      char buf[COUNTER_LENGTH+1];
      memcpy(buf, input[0]->str, COUNTER_LENGTH);
      buf[COUNTER_LENGTH] = 0;
      guchar *tempInt = convertToBin(buf, &outLen);
      if (outLen!=sizeof(guint64))
        {
          msg_warning("[SLOG] WARNING: Cannot derive integer value from first input line counter");
          (*startingEntry) = 0UL;
          g_free(tempInt);
          return 0;
        }
      else
        {
          memcpy(startingEntry, tempInt, sizeof(guint64));
          g_free(tempInt);
        }

      if((*startingEntry) > 0)
        {
          msg_warning("[SLOG] WARNING: Log does not start with index 0",
                      evt_tag_long("index", (*startingEntry)));
          (*nextLogEntry) = (*startingEntry);
          deriveKey(mainKey, (*nextLogEntry), 0);
          return 0;
        }
    }
  else
    {
      msg_warning("[SLOG] WARNING: Problems reading log entry at first line.");
      return 0;
    }

  return 1;
}


/*
 * Iteratively verify the integrity of an existing log file
 *
 * Return:
 * 1 on success
 * 0 on error
 */
int iterativeFileVerify(unsigned char *previousMAC, unsigned char *mainKey, char *inputFileName, unsigned char *bigMAC,
                        char *outputFileName, guint64 entriesInFile, int chunkLength, guint64 keyNumber)
{

  if(entriesInFile==0)
    {
      msg_error("[SLOG] ERROR: Nothing to verify");
      return 0;
    }

  unsigned char keyZero[KEY_LENGTH];
  memcpy(keyZero, mainKey, KEY_LENGTH);
  int startedWithZero = 0;

  if (keyNumber!=0)
    {
      msg_info("[SLOG] INFO: Verification using a key different from k0", evt_tag_long("key number", keyNumber));
    }
  else
    {
      msg_info("[SLOG] INFO: Verification starting with k0. Is this really what you want?");
      startedWithZero = 1;
    }
  int ret = 1;

  GError *myError = NULL;
  GIOChannel *input = g_io_channel_new_file(inputFileName, "r", &myError);
  if (!input)
    {
      cond_msg_error (myError, "[SLOG] ERROR: Cannot open input file");

      g_clear_error(&myError);

      return 0;
    }

  GIOStatus status = g_io_channel_set_encoding(input, NULL, &myError);
  if(status != G_IO_STATUS_NORMAL)
    {
      cond_msg_error (myError, "[SLOG] ERROR: set encoding for input file");

      g_clear_error(&myError);

      g_io_channel_shutdown(input, TRUE, &myError);
      g_io_channel_unref(input);

      g_clear_error(&myError);

      return 0;
    }

  GIOChannel *output = g_io_channel_new_file(outputFileName, "w+", &myError);

  if (!output)
    {
      cond_msg_error(myError, "[SLOG] ERROR: Cannot open output file");

      g_clear_error(&myError);

      g_io_channel_shutdown(input, TRUE, &myError);
      g_io_channel_unref(input);

      g_clear_error(&myError);

      return 0;
    }
  status = g_io_channel_set_encoding(output, NULL, &myError);
  if (status != G_IO_STATUS_NORMAL)
    {
      cond_msg_error(myError, "[SLOG] ERROR: Cannot set output file encoding");
      g_clear_error(&myError);

      g_io_channel_shutdown(input, TRUE, &myError);
      g_io_channel_unref(input);

      g_clear_error(&myError);

      g_io_channel_shutdown(output, TRUE, &myError);
      g_io_channel_unref(output);

      g_clear_error(&myError);

      return  0;
    }


  GString **inputBuffer = g_new0(GString *, chunkLength);
  GString **outputBuffer = g_new0(GString *, chunkLength);

  if ((outputBuffer==NULL)||(inputBuffer == NULL))
    {
      msg_error("[SLOG] ERROR: [iterativeFileVerify] cannot allocate memory");

      g_io_channel_shutdown(input, TRUE, &myError);
      g_io_channel_unref(input);
      g_clear_error(&myError);

      g_io_channel_shutdown(output, TRUE, &myError);
      g_io_channel_unref(output);
      g_clear_error(&myError);

      g_free(inputBuffer);
      g_free(outputBuffer);

      return 0;
    }

  unsigned char cmac_tag[CMAC_LENGTH];
  memcpy(cmac_tag, previousMAC, CMAC_LENGTH);

  guint64 nextLogEntry = keyNumber;
  guint64 startingEntry = keyNumber;
  guint64 numberOfLogEntries = keyNumber;

  // This is only to avoid updating BigMAC during the first iteration
  if (keyNumber == 0)
    {
      numberOfLogEntries = 1;
    }

  if (chunkLength>entriesInFile)
    {
      chunkLength = entriesInFile;
    }

  // Create the hash table
  GHashTable *tab = g_hash_table_new(g_str_hash, g_str_equal);
  if (tab == NULL)
    {
      msg_error("[SLOG] ERROR: Cannot create hash table");
      return 0;
    }

  // Process file in chunks
  for (int j = 0; j < (entriesInFile / chunkLength); j++)
    {
      for (guint64 i = 0; i < chunkLength; i++)
        {
          inputBuffer[i] = g_string_new(NULL);
          status = g_io_channel_read_line_string(input, inputBuffer[i], NULL, &myError);
          if (status != G_IO_STATUS_NORMAL)
            {
              cond_msg_error(myError, "[SLOG] ERROR: reading from input file");

              g_clear_error(&myError);

              g_io_channel_shutdown(input, TRUE, &myError);
              g_io_channel_unref(input);

              g_clear_error(&myError);

              g_io_channel_shutdown(output, TRUE, &myError);
              g_io_channel_unref(output);

              g_clear_error(&myError);

              g_free(inputBuffer);
              g_free(outputBuffer);

              return  0;
            }

          // Cut last character to remove the trailing new line...
          g_string_truncate(inputBuffer[i], (inputBuffer[i]->len) - 1);
        }
      ret = ret * iterateBuffer(chunkLength, inputBuffer, &nextLogEntry, mainKey, keyZero, keyNumber, outputBuffer,
                                &numberOfLogEntries, cmac_tag, tab);

      // ...and write to file
      for (guint64 i = 0; i < chunkLength; i++)
        {
          if (outputBuffer[i]->len!=0)
            {
              gsize size;
              //Add newline
              g_string_append(outputBuffer[i], "\n");
              status = g_io_channel_write_chars(output, (outputBuffer[i])->str, (outputBuffer[i])->len, &size, &myError);

              if (status != G_IO_STATUS_NORMAL)
                {
                  cond_msg_error(myError, "[SLOG] ERROR: writing to output file");

                  g_clear_error(&myError);

                  g_io_channel_shutdown(input, TRUE, &myError);
                  g_io_channel_unref(input);

                  g_clear_error(&myError);

                  g_io_channel_shutdown(output, TRUE, &myError);
                  g_io_channel_unref(output);

                  g_clear_error(&myError);

                  g_free(inputBuffer);
                  g_free(outputBuffer);

                  return  0;
                }
            }
          g_string_free(outputBuffer[i], TRUE);
          g_string_free(inputBuffer[i], TRUE);
        }
    }

  if ((entriesInFile % chunkLength) > 0)
    {
      for (guint64 i = 0; i < (entriesInFile % chunkLength); i++)
        {
          inputBuffer[i] = g_string_new(NULL);
          status = g_io_channel_read_line_string(input, inputBuffer[i], NULL, &myError);
          if (status != G_IO_STATUS_NORMAL)
            {
              cond_msg_error(myError, "[SLOG] ERROR: reading from input file");

              g_clear_error(&myError);

              g_io_channel_shutdown(input, TRUE, &myError);
              g_io_channel_unref(input);

              g_clear_error(&myError);

              g_io_channel_shutdown(output, TRUE, &myError);
              g_io_channel_unref(output);

              g_clear_error(&myError);

              g_free(inputBuffer);
              g_free(outputBuffer);


              return  0;
            }

          // Cut last character to remove the trailing new line
          g_string_truncate(inputBuffer[i], (inputBuffer[i]->len) - 1);
        }
      ret = ret * iterateBuffer((entriesInFile % chunkLength), inputBuffer, &nextLogEntry, mainKey, keyZero, keyNumber,
                                outputBuffer, &numberOfLogEntries, cmac_tag, tab);

      for (guint64 i = 0; i < (entriesInFile % chunkLength); i++)
        {
          if (outputBuffer[i]->len!=0)
            {

              gsize size;

              //Add newline
              g_string_append(outputBuffer[i], "\n");
              status = g_io_channel_write_chars(output, (outputBuffer[i])->str, (outputBuffer[i])->len, &size, &myError);
              if (status != G_IO_STATUS_NORMAL)
                {
                  cond_msg_error(myError, "[SLOG] ERROR: writing to output file");

                  g_clear_error(&myError);

                  g_io_channel_shutdown(input, TRUE, &myError);
                  g_io_channel_unref(input);

                  g_clear_error(&myError);

                  g_io_channel_shutdown(output, TRUE, &myError);
                  g_io_channel_unref(output);

                  g_clear_error(&myError);

                  g_free(inputBuffer);
                  g_free(outputBuffer);

                  return  0;
                }

            }
          g_string_free(outputBuffer[i], TRUE);
          g_string_free(inputBuffer[i], TRUE);
        }
    }

  if (startedWithZero==1)
    {
      msg_info("[SLOG] INFO: We started with key key0. There might be a lot of warnings about missing log entries.");
    }

  ret = ret * finalizeVerify(startingEntry, entriesInFile, bigMAC, cmac_tag, tab);

  g_free(inputBuffer);
  g_free(outputBuffer);

  g_io_channel_shutdown(input, TRUE, &myError);
  g_io_channel_unref(input);

  g_clear_error(&myError);

  g_io_channel_shutdown(output, TRUE, &myError);
  g_io_channel_unref(output);

  g_clear_error(&myError);

  return ret;

}

/*
 * Verify the integrity of an existing log file
 *
 * Return:
 * 1 on success
 * 0 on error
 */
int fileVerify(unsigned char *mainKey, char *inputFileName, char *outputFileName, unsigned char *bigMac,
               guint64 entriesInFile, int chunkLength)
{

  unsigned char keyZero[KEY_LENGTH];
  memcpy(keyZero, mainKey, KEY_LENGTH);

  GHashTable *tab = NULL;

  int ret = 1;

  if(entriesInFile==0)
    {
      msg_error("[SLOG] ERROR: Nothing to verify");
      return 0;
    }

  GError *myError = NULL;
  GIOChannel *input = g_io_channel_new_file(inputFileName, "r", &myError);
  if (!input)
    {
      cond_msg_error (myError, "[SLOG] ERROR: Cannot open input file");

      g_clear_error(&myError);

      return 0;
    }

  GIOStatus status =  g_io_channel_set_encoding(input, NULL, &myError);
  if (status != G_IO_STATUS_NORMAL)
    {
      cond_msg_error (myError, "[SLOG] ERROR: Cannot set input file encoding");

      g_clear_error(&myError);

      g_io_channel_shutdown(input, TRUE, &myError);
      g_io_channel_unref(input);

      g_clear_error(&myError);

      return 0;
    }

  GIOChannel *output = g_io_channel_new_file(outputFileName, "w+", &myError);

  if (!output)
    {
      cond_msg_error (myError, "[SLOG] ERROR: Cannot open output file");

      g_clear_error(&myError);

      g_io_channel_shutdown(input, TRUE, &myError);
      g_io_channel_unref(input);

      g_clear_error(&myError);


      return 0;
    }

  status = g_io_channel_set_encoding(output, NULL, &myError);

  if (status != G_IO_STATUS_NORMAL)
    {
      cond_msg_error (myError, "[SLOG] ERROR: Cannot set output file encoding");

      g_clear_error(&myError);

      g_io_channel_shutdown(input, TRUE, &myError);
      g_io_channel_unref(input);

      g_clear_error(&myError);

      g_io_channel_shutdown(output, TRUE, &myError);
      g_io_channel_unref(output);

      g_clear_error(&myError);

      return 0;
    }

  GString **inputBuffer = g_new0(GString *, chunkLength);
  GString **outputBuffer = g_new0(GString *, chunkLength);

  if ((outputBuffer==NULL)||(inputBuffer == NULL))
    {
      msg_error("[SLOG] ERROR: [fileVerify] cannot allocate memory");

      g_io_channel_shutdown(input, TRUE, &myError);
      g_io_channel_unref(input);

      g_clear_error(&myError);

      g_io_channel_shutdown(output, TRUE, &myError);
      g_io_channel_unref(output);

      g_clear_error(&myError);

      return 0;
    }

  guint64 nextLogEntry = 0UL;
  guint64 startingEntry = 0UL;
  unsigned char cmac_tag[CMAC_LENGTH];
  guint64 numberOfLogEntries = 0UL;

  if (chunkLength>entriesInFile)
    {
      chunkLength = entriesInFile;
    }

  for (guint64 i = 0; i < chunkLength; i++)
    {
      inputBuffer[i] = g_string_new(NULL);
      status = g_io_channel_read_line_string(input, inputBuffer[i], NULL, &myError);
      if (status != G_IO_STATUS_NORMAL)
        {
          cond_msg_error (myError, "[SLOG] ERROR: Cannot read from input file");

          g_clear_error(&myError);

          g_io_channel_shutdown(input, TRUE, &myError);
          g_io_channel_unref(input);

          g_clear_error(&myError);

          g_io_channel_shutdown(output, TRUE, &myError);
          g_io_channel_unref(output);

          g_clear_error(&myError);

          g_free(inputBuffer);
          g_free(outputBuffer);

          return 0;
        }

      // Cut last character to remove the trailing new line
      g_string_truncate(inputBuffer[i], (inputBuffer[i]->len) - 1);
    }

  ret = ret * initVerify(entriesInFile, mainKey, &nextLogEntry, &startingEntry, inputBuffer, &tab);
  ret = ret * iterateBuffer(chunkLength, inputBuffer, &nextLogEntry, mainKey, keyZero, 0, outputBuffer,
                            &numberOfLogEntries, cmac_tag, tab);

  // Write to file
  for (guint64 i = 0; i < chunkLength; i++)
    {
      if (outputBuffer[i]->len!=0)
        {
          gsize size;
          //Add newline
          g_string_append(outputBuffer[i], "\n");
          status = g_io_channel_write_chars(output, (outputBuffer[i])->str, (outputBuffer[i])->len, &size, &myError);
          if (status != G_IO_STATUS_NORMAL)
            {
              cond_msg_error (myError, "[SLOG] ERROR: writing to output file");
              g_clear_error(&myError);

              g_io_channel_shutdown(input, TRUE, &myError);
              g_io_channel_unref(input);

              g_clear_error(&myError);

              g_io_channel_shutdown(output, TRUE, &myError);
              g_io_channel_unref(output);

              g_clear_error(&myError);

              g_free(inputBuffer);
              g_free(outputBuffer);

              return 0;
            }
        }

      g_string_free(outputBuffer[i], TRUE);
      g_string_free(inputBuffer[i], TRUE);

    }

  // Process file in chunks
  for (int j = 0; j<(entriesInFile/chunkLength)-1; j++)
    {
      for (guint64 i = 0; i < chunkLength; i++)
        {
          inputBuffer[i] = g_string_new(NULL);
          status = g_io_channel_read_line_string(input, inputBuffer[i], NULL, &myError);
          if (status != G_IO_STATUS_NORMAL)
            {
              cond_msg_error (myError, "[SLOG] ERROR: Cannot read from input file");
              g_clear_error(&myError);

              g_io_channel_shutdown(input, TRUE, &myError);
              g_io_channel_unref(input);

              g_clear_error(&myError);

              g_io_channel_shutdown(output, TRUE, &myError);
              g_io_channel_unref(output);

              g_clear_error(&myError);

              g_free(inputBuffer);
              g_free(outputBuffer);

              return 0;
            }
          // Cut last character to remove the trailing new line...
          g_string_truncate(inputBuffer[i], (inputBuffer[i]->len) - 1);
        }
      ret = ret * iterateBuffer(chunkLength, inputBuffer, &nextLogEntry, mainKey, keyZero, 0, outputBuffer,
                                &numberOfLogEntries, cmac_tag, tab);

      // ...and write to file
      for (guint64 i = 0; i < chunkLength; i++)
        {
          if (outputBuffer[i]->len!=0)
            {
              gsize size;
              //Add newline
              g_string_append(outputBuffer[i], "\n");
              status = g_io_channel_write_chars(output, (outputBuffer[i])->str, (outputBuffer[i])->len, &size, &myError);
              if (status != G_IO_STATUS_NORMAL)
                {
                  cond_msg_error (myError, "[SLOG] ERROR: writing to output file");

                  g_clear_error(&myError);

                  g_io_channel_shutdown(input, TRUE, &myError);
                  g_io_channel_unref(input);

                  g_clear_error(&myError);

                  g_io_channel_shutdown(output, TRUE, &myError);
                  g_io_channel_unref(output);

                  g_clear_error(&myError);

                  g_free(inputBuffer);
                  g_free(outputBuffer);

                  return 0;
                }

            }
          g_string_free(outputBuffer[i], TRUE);
          g_string_free(inputBuffer[i], TRUE);
        }
    }

  if ((entriesInFile % chunkLength) > 0)
    {
      for (guint64 i = 0; i < (entriesInFile % chunkLength); i++)
        {
          inputBuffer[i] = g_string_new(NULL);
          status = g_io_channel_read_line_string(input, inputBuffer[i], NULL, &myError);
          if (status != G_IO_STATUS_NORMAL)
            {
              cond_msg_error (myError, "[SLOG] ERROR: Cannot read from input file");
              g_clear_error(&myError);

              g_io_channel_shutdown(input, TRUE, &myError);
              g_io_channel_unref(input);

              g_clear_error(&myError);

              g_io_channel_shutdown(output, TRUE, &myError);
              g_io_channel_unref(output);

              g_clear_error(&myError);

              g_free(inputBuffer);
              g_free(outputBuffer);

              return 0;
            }
          // Cut last character to remove the trailing new line
          g_string_truncate(inputBuffer[i], (inputBuffer[i]->len) - 1);
        }
      ret = ret * iterateBuffer((entriesInFile % chunkLength), inputBuffer, &nextLogEntry, mainKey, keyZero, 0, outputBuffer,
                                &numberOfLogEntries, cmac_tag, tab);

      for (guint64 i = 0; i < (entriesInFile % chunkLength); i++)
        {
          if (outputBuffer[i]->len!=0)
            {

              gsize size;
              //Add newline
              g_string_append(outputBuffer[i], "\n");
              status = g_io_channel_write_chars(output, (outputBuffer[i])->str, (outputBuffer[i])->len, &size, &myError);
              if (status != G_IO_STATUS_NORMAL)
                {
                  cond_msg_error (myError, "[SLOG] ERROR: Cannot write to output file");
                  g_clear_error(&myError);

                  g_io_channel_shutdown(input, TRUE, &myError);
                  g_io_channel_unref(input);

                  g_clear_error(&myError);

                  g_io_channel_shutdown(output, TRUE, &myError);
                  g_io_channel_unref(output);

                  g_clear_error(&myError);

                  g_free(inputBuffer);
                  g_free(outputBuffer);

                  return 0;
                }
            }
          g_string_free(outputBuffer[i], TRUE);
          g_string_free(inputBuffer[i], TRUE);
        }

    }

  ret = ret * finalizeVerify(startingEntry, entriesInFile, bigMac, cmac_tag, tab);

  g_free(inputBuffer);
  g_free(outputBuffer);

  g_io_channel_shutdown(input, TRUE, &myError);
  g_io_channel_unref(input);

  g_clear_error(&myError);

  g_io_channel_shutdown(output, TRUE, &myError);
  g_io_channel_unref(output);

  g_clear_error(&myError);

  return ret;
}

// Print usage message and clean up
int slog_usage(GOptionContext *ctx, GOptionGroup *grp, GString *errormsg)
{
  if(errormsg != NULL)
    {
      g_print ("\nERROR: %s\n\n", errormsg->str);
      g_string_free(errormsg, TRUE);
    }

  g_print("%s", g_option_context_get_help(ctx, TRUE, NULL));
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

  for(SLogOptions *option = opts; option != NULL && option->longname != NULL; option++)
    {
      g_string_append(longOption, option->longname);
      g_string_append_c(shortOption, option->shortname);

      if(g_string_equal(currentOption, longOption) || g_string_equal(currentOption, shortOption))
        {
          if(g_file_test(value, G_FILE_TEST_IS_REGULAR))
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

