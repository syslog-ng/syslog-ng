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

#ifndef SLOG_H_INCLUDED
#define SLOG_H_INCLUDED 1

#define AES_BLOCKSIZE 16
#define IV_LENGTH 12
#define KEY_LENGTH 32
#define CMAC_LENGTH 16
#define KEY_ERROR 0x20
#define KEY_FILE_ERROR 0x20
#define KEY_READ_ERROR 0x21
#define KEY_WRITE_ERROR 0x22
#define COLON 1
#define BLANK 1
#define COUNTER_LENGTH 12 // We use an 8 byte counter resulting in 12 byte BASE64 encoding
#define CTR_LEN_SIMPLE 16 // This is for 8 byte (=2^64) counters; simple encoding by doubling

// These are arbitrary constants (with mean) Hamming distance.
#define IPAD 0x36
#define OPAD 0x5C
#define EPAD 0x6A

// Buffer size for import and verification
#define MIN_BUF_SIZE 10
#define MAX_BUF_SIZE 1073741823 // INT_MAX/2
#define DEF_BUF_SIZE 1000

// Error message in case of invalid file
#define FILE_ERROR "Invalid path or non existing regular file: "

// Structure for command line arguments of template and utilities
typedef struct
{
  char *longname;
  char shortname;
  char *description;
  char *type;
  char *arg;
} SLogOptions;

// Dump contents of an array on STDOUT, byte by byte, converting to hex.
void outputByteBuffer(unsigned char *buf, int length);


void evolveKey(unsigned char *key);

/*
 *  Encrypts plaintext
 *
 * 1. Parameter: pointer to plaintext (input)
 * 2. Parameter: length of plaintext (input)
 * 3. Parameter: pointer to key (input)
 * 4. Parameter: pointer to IV (input, nonce of length IV_LENGTH)
 * 5. Parameter: pointer to ciphertext (output)
 * 6. Parameter: pointer to tag (output)
 *
 * Note: Caller must take care of memory management.
 *
 * Return:
 * Length of ciphertext (>0)
 * 0 on error
 */
int sLogEncrypt(unsigned char *plaintext, int plaintext_len,
                unsigned char *key, unsigned char *iv,
                unsigned char *ciphertext, unsigned char *tag);
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
                unsigned char *plaintext);

/*
 * Compute AES256 CMAC of input
 *
 *
 * 1. Parameter: Pointer to key (input)
 * 2. Parameter: Pointer to input (input)
 * 3. Parameter: Input length (input)
 * 4. Parameter: Pointer to output (output)
 * 5. Parameter: Length of output (output)
 *
 * If Parameter 5 == 0, there was an error.
 *
 * Note: Caller must take care of memory management.
 */
int sLogDecrypt(unsigned char *ciphertext, int ciphertext_len, unsigned char *tag, unsigned char *key,
                unsigned char *iv,
                unsigned char *plaintext);

void cmac(unsigned char *key, const void *input, gsize length, unsigned char *out, gsize *outlen);


gchar *convertToBase64(unsigned char *input, gsize len);
guchar *convertToBin(char *input, gsize *outLen);

/*
 * Derive key = evolve key multiple times
 *
 * 1. Parameter: Pointer to destination key (output)
 * 2. Parameter: Number of times current key should be evolved (input)
 * 3. Parameter: Pointer to current key (input)
 *
 *
 * Note: Caller must take care of memory management.
 */
void deriveKey(unsigned char *dst, guint64 index, guint64 currentKey);

/*
 * Create a new encrypted log entry
 *
 * This function creates a new encrypted log entry updates the corresponding MAC accordingly
 *
 * 1. Parameter: Number of log entries (for enumerating the entries in the log file)
 * 2. Parameter: The original log message
 * 3. Parameter: The current encryption key
 * 4. Parameter: The current MAC
 * 5. Parameter: The resulting encrypted log entry
 * 6. Parameter: The newly updated MAC
 */
void sLogEntry(guint64 numberOfLogEntries, GString *text, unsigned char *key, unsigned char *inputBigMac,
               GString *output, unsigned char *outputBigMac);

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
int generateMasterKey(guchar *masterkey);

/*
 * Generate a host key based on a previously created master key
 *
 * 1. Parameter: Master key
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
int deriveHostKey(guchar *masterkey, gchar *macAddr, gchar *serial, guchar *hostkey);

int readBigMAC(gchar *filename, char *outputBuffer);
int writeBigMAC(gchar *filename, char *outputBuffer);

/*
 * Read key from file
 *
 * Return:
 * 1 on success
 * 0 on error
 */
int readKey(char *destKey, guint64 *destCounter, gchar *keypath);

/*
 * Write key to file
 *
 * Return:
 * 1 on success
 * 0 on error
 */
int writeKey(char *key, guint64 counter, gchar *keypath);

/*
 * Verify the integrity of an existing log file
 *
 * Return:
 * 1 on success
 * 0 on error
 */
int fileVerify(unsigned char *key, char *inputFileName, char *outputFileName, unsigned char *bigMac,
               guint64 entriesInFile, int chunkLength);

int initVerify(guint64 entriesInFile, unsigned char *key, guint64 *nextLogEntry, guint64 *startingEntry,
               GString **input, GHashTable **tab);

int iterateBuffer(guint64 entriesInBuffer, GString **input, guint64 *nextLogEntry, unsigned char *key,
                  unsigned char *keyZero, guint keyNumber, GString **output, guint64 *numberOfLogEntries, unsigned char *cmac_tag,
                  GHashTable *tab);

int finalizeVerify(guint64 startingEntry, guint64 entriesInFile, unsigned char *bigMac, unsigned char *cmac_tag,
                   GHashTable *tab);

int iterativeFileVerify(unsigned char *previousMAC, unsigned char *previousKey, char *inputFileName,
                        unsigned char *currentMAC, char *outputFileName, guint64 entriesInFile, int chunkLength, guint64 keyNumber);

void deriveEncSubKey(unsigned char *mainKey, unsigned char *encKey);
void deriveMACSubKey(unsigned char *mainKey, unsigned char *MACKey);
void PRF(unsigned char *key, unsigned char *originalInput, guint64 inputLength, unsigned char *output,
         guint64 outputLength);

// Print usage message and clean up
int slog_usage(GOptionContext *ctx, GOptionGroup *grp, GString *errormsg);

/*
 * Callback function to check whether a command line argument represents a valid file name
 *
 * Return:
 * TRUE on success
 * FALSE on error
 */
gboolean validFileNameArg(const gchar *option_name, const gchar *value, gpointer data, GError **error);

#endif
