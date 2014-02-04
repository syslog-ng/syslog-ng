#include "pathutils.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "libtest/testutils.h"
#include <unistd.h>

void
test_is_directory_return_false_in_case_of_regular_file(void)
{
  int fd = open("test.file", O_CREAT, 0644);
  write(fd, "a", 1);
  close(fd);

  assert_false(is_file_directory("test.file"), "File is not a directory!");

  unlink("test.file");
}

int
main()
{
  test_is_directory_return_false_in_case_of_regular_file();
}
