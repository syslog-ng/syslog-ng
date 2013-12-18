#include "testutils.h"
#include "affile/affile-common.h"
#include "lib/messages.h"
#include <fcntl.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#define AFFILE_TESTCASE(testfunc, ...) { testcase_begin("%s(%s)", #testfunc, #__VA_ARGS__); testfunc(__VA_ARGS__); testcase_end(); }

#define CREATE_DIRS 0x01
#define PIPE 0x02

#define TEST_DIR "affile_test_dir"

#define PIPE_OPEN_FLAGS (O_RDWR | O_NOCTTY | O_NONBLOCK | O_LARGEFILE)
#define REGULAR_FILE_OPEN_FLAGS (O_CREAT | O_NOCTTY | O_LARGEFILE)

static void
setup()
{
  msg_init(FALSE);
}

static mode_t
get_fd_file_mode(gint fd)
{
  struct stat st;

  fstat(fd, &st);
  return st.st_mode;
}

static gboolean
open_file(gchar *fname, int open_flags, gint extra_flags, gint *fd)
{
  FilePermOptions perm_opts;
  FileOpenOptions open_opts;

  file_perm_options_defaults(&perm_opts);

  open_opts.open_flags = open_flags;
  open_opts.create_dirs = !!(extra_flags & CREATE_DIRS);
  open_opts.is_pipe = !!(extra_flags & PIPE);
  open_opts.needs_privileges = FALSE;

  return affile_open_file(fname, &open_opts, &perm_opts, fd);
}

static void
test_open_regular_file()
{
  gint fd;
  gchar fname[] = "test.log";

  assert_true(open_file(fname, REGULAR_FILE_OPEN_FLAGS, 0, &fd), "affile_open_file failed: %s", fname);
  assert_true(S_ISREG(get_fd_file_mode(fd)) != 0, "%s is not regular file", fname);

  close(fd);
  remove(fname);
}

static void
test_open_pipe()
{
  gint fd;
  gchar fname[] = "test.pipe";

  assert_true(open_file(fname, PIPE_OPEN_FLAGS, PIPE, &fd), "failed to open %s", fname);
  assert_true(S_ISFIFO(get_fd_file_mode(fd)) != 0, "%s is not pipe", fname);

  close(fd);
  remove(fname);
}

static void
test_spurious_path()
{
  gint fd;
  gchar fname[] = "./../test.fname";

  assert_false(open_file(fname, REGULAR_FILE_OPEN_FLAGS, 0, &fd), "affile_open_file should not be able to open: %s", fname);
}

static void
test_create_file_in_nonexistent_dir()
{
  gchar test_dir[] = "nonexistent";
  gchar fname[] = "nonexistent/test.txt";
  gint fd;

  assert_false(open_file(fname, REGULAR_FILE_OPEN_FLAGS, 0, &fd), "affile_open_file failed: %s", fname);
  assert_true(open_file(fname, REGULAR_FILE_OPEN_FLAGS, CREATE_DIRS, &fd), "affile_open_file failed: %s", fname);

  close(fd);
  remove(fname);
  rmdir(test_dir);
}

static void
test_file_flags()
{
  gint fd;
  gchar fname[] = "test_flags.log";
  gint flags = O_CREAT|O_WRONLY;

  assert_true(open_file(fname, flags, 0, &fd), "affile_open_file failed: %s", fname);
  assert_true((fcntl(fd, F_GETFL) & O_WRONLY) == O_WRONLY, "invalid open flags");

  close(fd);
  remove(fname);
}

int main(int argc, char **argv)
{
  setup();

  AFFILE_TESTCASE(test_open_regular_file);
  AFFILE_TESTCASE(test_open_pipe);
  AFFILE_TESTCASE(test_spurious_path);
  AFFILE_TESTCASE(test_create_file_in_nonexistent_dir);
  AFFILE_TESTCASE(test_file_flags);
}
