#include "logproto/logproto-server.h"
#include "logmsg.h"
#include <stdlib.h>

static void
testcase(const gchar *msg_, gsize msg_len, gint eom_ofs)
{
  const guchar *eom;
  const guchar *msg = (const guchar *) msg_;

  eom = find_eom((guchar *) msg, msg_len);

  if (eom_ofs == -1 && eom != NULL)
    {
      fprintf(stderr, "EOM returned is not NULL, which was expected. eom_ofs=%d, eom=%s\n", eom_ofs, eom);
      exit(1);
    }
  if (eom_ofs == -1)
    return;

  if (eom - msg != eom_ofs)
    {
      fprintf(stderr, "EOM is at wrong location. msg=%s, eom_ofs=%d, eom=%s\n", msg, eom_ofs, eom);
      exit(1);
    }
}

int
main()
{
  testcase("a\nb\nc\n",  6,  1);
  testcase("ab\nb\nc\n",  7,  2);
  testcase("abc\nb\nc\n",  8,  3);
  testcase("abcd\nb\nc\n",  9,  4);
  testcase("abcde\nb\nc\n", 10,  5);
  testcase("abcdef\nb\nc\n", 11,  6);
  testcase("abcdefg\nb\nc\n", 12,  7);
  testcase("abcdefgh\nb\nc\n", 13,  8);
  testcase("abcdefghi\nb\nc\n", 14,  9);
  testcase("abcdefghij\nb\nc\n", 15, 10);
  testcase("abcdefghijk\nb\nc\n", 16, 11);
  testcase("abcdefghijkl\nb\nc\n", 17, 12);
  testcase("abcdefghijklm\nb\nc\n", 18, 13);
  testcase("abcdefghijklmn\nb\nc\n", 19, 14);
  testcase("abcdefghijklmno\nb\nc\n", 20, 15);
  testcase("abcdefghijklmnop\nb\nc\n", 21, 16);
  testcase("abcdefghijklmnopq\nb\nc\n", 22, 17);
  testcase("abcdefghijklmnopqr\nb\nc\n", 23, 18);
  testcase("abcdefghijklmnopqrs\nb\nc\n", 24, 19);
  testcase("abcdefghijklmnopqrst\nb\nc\n", 25, 20);
  testcase("abcdefghijklmnopqrstu\nb\nc\n", 26, 21);
  testcase("abcdefghijklmnopqrstuv\nb\nc\n", 27, 22);
  testcase("abcdefghijklmnopqrstuvw\nb\nc\n", 28, 23);
  testcase("abcdefghijklmnopqrstuvwx\nb\nc\n", 29, 24);
  testcase("abcdefghijklmnopqrstuvwxy\nb\nc\n", 30, 25);
  testcase("abcdefghijklmnopqrstuvwxyz\nb\nc\n", 31, 26);

  testcase("a",  1, -1);
  testcase("ab",  2, -1);
  testcase("abc",  3, -1);
  testcase("abcd",  4, -1);
  testcase("abcde",  5, -1);
  testcase("abcdef",  6, -1);
  testcase("abcdefg",  7, -1);
  testcase("abcdefgh",  8, -1);
  testcase("abcdefghi",  9, -1);
  testcase("abcdefghij", 10, -1);
  testcase("abcdefghijk", 11, -1);
  testcase("abcdefghijkl", 12, -1);
  testcase("abcdefghijklm", 13, -1);
  testcase("abcdefghijklmn", 14, -1);
  testcase("abcdefghijklmno", 15, -1);
  testcase("abcdefghijklmnop", 16, -1);
  testcase("abcdefghijklmnopq", 17, -1);
  testcase("abcdefghijklmnopqr", 18, -1);
  testcase("abcdefghijklmnopqrs", 19, -1);
  testcase("abcdefghijklmnopqrst", 20, -1);
  testcase("abcdefghijklmnopqrstu", 21, -1);
  testcase("abcdefghijklmnopqrstuv", 22, -1);
  testcase("abcdefghijklmnopqrstuvw", 23, -1);
  testcase("abcdefghijklmnopqrstuvwx", 24, -1);
  testcase("abcdefghijklmnopqrstuvwxy", 25, -1);
  testcase("abcdefghijklmnopqrstuvwxyz", 26, -1);
  return 0;
}
