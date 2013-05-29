#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <string.h>
#include <ctype.h>

#define NONCE_LEN 32
#define MD_LEN    SHA_DIGEST_LENGTH

static const unsigned char nonces[][NONCE_LEN] =
{
  {
    0x5d, 0xf0, 0x5a, 0xc1, 0x08, 0x7c, 0x54, 0xea, 0x65, 0x56, 0xf7, 0x22, 0x79, 0xca, 0x01, 0x0a,
    0x31, 0x74, 0x3d, 0xcc, 0x07, 0xf5, 0x97, 0x57, 0x7b, 0xec, 0x9f, 0xbf, 0xcf, 0xa0, 0x29, 0x70
  }
};


int
check_signature(const char *fingerprint, int nonce_index, const char *signature)
{
  HMAC_CTX hmac;
  char new_signature[MD_LEN];

  if (nonce_index < 0 || nonce_index > sizeof(nonces[0]))
    {
      fprintf(stderr, "Unknown nonce_index, you probably need a newer logchksign program. nonce_index=%d\n", nonce_index);
      return 0;
    }

  HMAC_CTX_init(&hmac);
  HMAC_Init(&hmac, nonces[nonce_index], NONCE_LEN, EVP_sha1());
  HMAC_Update(&hmac, (unsigned char *) fingerprint, MD_LEN);
  HMAC_Final(&hmac, (unsigned char *) new_signature, NULL);
  HMAC_CTX_cleanup(&hmac);

  if (memcmp(new_signature, signature, MD_LEN) != 0)
    {
      fprintf(stderr, "Invalid signature.\n");
      return 0;
    }
  return 1;
}

const char *
find_value(const char *msg, const char *pattern)
{
  const char *start;

  start = strstr(msg, pattern);

  if (start)
    return start + strlen(pattern);
  return NULL;
}

int
xvalue(char xdigit)
{
  xdigit = toupper(xdigit);
  if (xdigit >= '0' && xdigit <= '9')
    return xdigit - '0';
  else if (xdigit >= 'A' && xdigit <= 'F')
    return xdigit - 'A' + 10;
  return -1;
}

int
hex2bin(const char *str, char *value)
{
  int i;

  if (*str != '\'')
    return 0;
  str++;
  i = 0;
  while (isxdigit(str[i]) && i < MD_LEN * 2)
    {
      int v;

      if (!isxdigit(str[i]))
        {
          fprintf(stderr, "Truncated hex string, or not a hex character: %c\n", str[i]);
          return 0;
        }
      v = xvalue(str[i]);
      if (v == -1)
        return 0;

      if ((i & 1) == 0)
        {
          /* even */
          value[i >> 1] = v << 4;
        }
      else
        {
          /* odd */
          value[i >> 1] |= v;
        }
      i++;
    }
  if (i != MD_LEN * 2)
    {
      fprintf(stderr, "Hex string too short, %d != %d\n", i, MD_LEN * 2);
      return 0;
    }
  if (str[i] != '\'')
    return 0;
  return 1;
}

int
str2int(const char *str, int *value)
{
  char *end;

  if (*str != '\'')
    return 0;
  str++;
  *value = strtol(str, &end, 10);
  if (*end != '\'')
    return 0;
  return 1;
}


int
check_message(const char *msg)
{
  const char *fp_str, *nonce_ndx_str, *sign_str;
  char fp[MD_LEN];
  char sign[MD_LEN];
  int nonce_ndx;

  fp_str = find_value(msg, "cfg-fingerprint=");
  nonce_ndx_str = find_value(msg, "cfg-nonce-ndx=");
  sign_str = find_value(msg, "cfg-signature=");

  if (!fp_str || !nonce_ndx_str || !sign_str)
    {
      fprintf(stderr, "Required parameters missing from log message.\n");
      return 0;
    }
  if (!hex2bin(fp_str, fp) ||
      !str2int(nonce_ndx_str, &nonce_ndx) ||
      !hex2bin(sign_str, sign))
    {
      fprintf(stderr, "Error parsing parameters from log message.\n");
      return 0;
    }
  return check_signature(fp, nonce_ndx, sign);
}

int
main(int argc, char *argv[])
{
  if (argc != 2)
    {
      fprintf(stderr, "Syntax: logchksign \"<message>\"\n");
      return 1;
    }
  if (check_message(argv[1]))
    {
      fprintf(stderr, "Signature OK\n");
      return 0;
    }
  return 1;
}
