#include "pathutils.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "libtest/testutils.h"
#include <unistd.h>

void
test_is_directory_return_false_in_case_of_regular_file(void)
{
  int fd = open("test.file", O_CREAT | O_RDWR, 0644);
  assert_false(fd < 0, "open error");
  ssize_t done = write(fd, "a", 1);
  assert_false(done != 1, "write error");
  int error = close(fd);
  assert_false(error, "close error");

  assert_false(is_file_directory("test.file"), "File is not a directory!");

  error = unlink("test.file");
  assert_false(error, "unlink error");
}

int
main()
{
  test_is_directory_return_false_in_case_of_regular_file();
}
