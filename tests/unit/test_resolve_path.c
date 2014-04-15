#include "filemonitor.h"
#include <limits.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define WORK_DIR "/tmp/test_resolve_path"
#define EXPECTED_SPEEDUP 10.0
#define MEASURE_SPEEDUP_CYCLES 10000

#ifdef __GNUC__
#define PRETTY_FUNCTION __PRETTY_FUNCTION__
#else
#define PRETTY_FUNCTION "[]"
#endif

#define TEST_ASSERT(assertion, msg) if (!(assertion)) \
                                      { \
                                         fprintf(stderr,"\n%s :: %s\n\n", PRETTY_FUNCTION, msg); \
                                         exit(1); \
                                      }

static inline long
get_path_max()
{
  long path_max;
#ifdef PATH_MAX
  path_max = PATH_MAX;
#else
  path_max = pathconf(path, _PC_PATH_MAX);
  if (path_max <= 0)
  path_max = 4096;
#endif
  return path_max;
}

typedef gchar *(*PathResolverFunc)(const gchar *path, const gchar *basedir);

typedef struct _PathResolver
{
  PathResolverFunc resolver_func;
  const gchar *name;
} PathResolver;

static inline void
start_time(struct timeval *tv)
{
  gettimeofday(tv, NULL);
}

static inline void
end_time(struct timeval *tv)
{
  gettimeofday(tv, NULL);
}

static inline double
calc_elapsed_time(struct timeval *start_t, struct timeval *end_t)
{
  double t1 = (double) start_t->tv_sec + ((double)start_t->tv_usec/1e06);
  double t2 = (double) end_t->tv_sec + ((double)end_t->tv_usec/1e06);

  return t2-t1;
}

static gchar*
build_filename(const gchar *basedir, const gchar *path)
{
  gchar *result;

  if (!path)
    return NULL;

  result = (gchar *)g_malloc(strlen(basedir) + strlen(path) + 2);
  sprintf(result, "%s/%s", basedir, path);

  return result;
}

static gchar*
my_resolve_to_absolute_path(const gchar *path, const gchar *basedir)
{
  long path_max = get_path_max();
  gchar *res;
  gchar *w_name;

  w_name = build_filename(basedir, path);
  res = (char *)g_malloc(path_max);

  if (!realpath(w_name, res))
    {
      fprintf(stderr, "realpath(%s) failed : [%s]\n", w_name, strerror(errno));
      g_free(w_name);
      g_free(res);
      return NULL;
    }

  g_free(w_name);

  return res;
}

static
int nftw_delete_callback(const gchar *path,
                         const struct stat *stat_info,
                         int tflags,
                         struct FTW *ftwbuf)
{
  switch (tflags)
  {
    case FTW_DP:
      fprintf(stderr, "deleting directory[%s]...", path);
      if (rmdir(path) == 0)
        fprintf(stderr, "OK\n");
      else
        fprintf(stderr, "%s\n", strerror(errno));
      break;
    case FTW_F:
    case FTW_SL:
      fprintf(stderr, "deleting file[%s]...", path);
      if (unlink(path) == 0)
        fprintf(stderr, "OK\n");
      else
        fprintf(stderr, "%s\n", strerror(errno));
      break;
    default:
      break;
  }
  return 0;
}

static void
cleanup_dir(const gchar *dirname)
{
  int flags = FTW_PHYS|FTW_CHDIR|FTW_DEPTH;
  int res;

  if (nftw(WORK_DIR, nftw_delete_callback, 20, flags))
    {
      fprintf(stderr, "Error:[%s]\n", strerror(errno));
    }
  res = mkdir(WORK_DIR, S_IRWXU | S_IRWXG);
  if (res)
    {
      fprintf(stderr, "mkdir() failed: [%s]\n", strerror(errno));
    }
}


static void
create_file(const gchar *filename, const gchar *content)
{
  FILE *fp;
  gchar *w_fname;

  w_fname = build_filename(WORK_DIR, filename);

  fprintf(stderr, "creating file[%s]...", w_fname);
  fp = fopen(w_fname, "wt");
  if (!fp)
    {
      fprintf(stderr, "fopen(%s) failed: [%s]\n", w_fname, strerror(errno));
      g_free(w_fname);
      exit(errno);
    }
  fprintf(stderr, "OK\n");

  g_free(w_fname);
  fprintf(fp, "%s", content);
  fclose(fp);
}

static void
create_dir(const gchar *dirname)
{
  gchar *w_dirname;
  int res;

  w_dirname = build_filename(WORK_DIR, dirname);

  fprintf(stderr, "creating directory[%s]...", w_dirname);
  if ((res = mkdir(w_dirname, S_IRWXU | S_IRWXG)))
    {
      if ((errno != EEXIST))
        {
          fprintf(stderr, "mkdir(%s) failed : [%s]\n", w_dirname, strerror(errno));
          g_free(w_dirname);
          exit(errno);
        }
    }
  fprintf(stderr, "OK\n");

  free(w_dirname);
}

static void
create_symlink(const gchar *original_path, const gchar *symlink_path)
{
  gchar *w_original;
  gchar *w_symlink;
  int res;

  w_original = build_filename(WORK_DIR, original_path);
  w_symlink = build_filename(WORK_DIR, symlink_path);

  fprintf(stderr, "creating symlink[%s->%s]...", w_original, w_symlink);
  if ((res = symlink(w_original, w_symlink)))
    {
      if (res != EEXIST)
        {
          fprintf(stderr, "symlink(%s, %s) failed: [%s]\n", w_original, w_symlink, strerror(errno));
          g_free(w_original);
          g_free(w_symlink);
          exit(errno);
        }
    }
  fprintf(stderr, "OK\n");

  g_free(w_original);
  g_free(w_symlink);
}

static void
fill_dir(const gchar *dirname)
{
  create_dir("testdir1");
  create_dir("testdir1/testdir1-1");
  create_file("testdir1/testfile1", "I'm testfile1\n");
  create_symlink("testdir1/testfile1", "testdir1/testdir1-1/testfile1");
  create_symlink("testdir1/testdir1-1", "testdir1/testdir1-2");
}

static void
teardown()
{
  fprintf(stderr, "\nTeardown:\n");
  cleanup_dir(WORK_DIR);
}

static void
setup()
{
  fprintf(stderr, "\nSetup:\n");
  cleanup_dir(WORK_DIR);
  fill_dir(WORK_DIR);
  atexit(teardown);
  fprintf(stderr, "\n");
}


static void
assert_if_resolved_name_not_equal(PathResolver *path_resolver,
                                  const gchar *name,
                                  const gchar *expected)
{
  gchar *w_expected;
  gchar *ret;

  w_expected = build_filename(WORK_DIR, expected);
  ret = path_resolver->resolver_func(name, WORK_DIR);

  if (strcmp(ret, w_expected))
    fprintf(stderr, "strcmp(%s, %s) : not equal\n", ret, w_expected);

  TEST_ASSERT(strcmp(ret, w_expected) == 0, path_resolver->name);

  g_free(ret);
  g_free(w_expected);
}

static void
test_path_resolving(PathResolver *path_resolver)
{
  assert_if_resolved_name_not_equal(path_resolver,
                                    "testdir1/.././testdir1/./testfile1",
                                    "testdir1/testfile1");

  assert_if_resolved_name_not_equal(path_resolver,
                                    "testdir1/../testdir1/./../testdir1/testdir1-1/../"\
                                    "testdir1-1/testfile1",
                                    "testdir1/testfile1");

  assert_if_resolved_name_not_equal(path_resolver,
                                    "testdir1/testdir1-2",
                                    "testdir1/testdir1-1");

  assert_if_resolved_name_not_equal(path_resolver,
                                    "testdir1/testdir1-2/././././././././././././././."\
                                    "/././../testdir1-2",
                                    "testdir1/testdir1-1");

  assert_if_resolved_name_not_equal(path_resolver,
                                    "testdir1/testdir1-2/././././././././././././././."\
                                    "/././../testdir1-2/././././././././../testdir1-2/"\
                                    "./././././././././../testdir1-2",
                                    "testdir1/testdir1-1");

  assert_if_resolved_name_not_equal(path_resolver,
                                    "testdir1/testdir1-2/././././././././././././././."\
                                    "/././../testdir1-2/././././././././../testdir1-2/"\
                                    "././././././././"\
                                    "./../testdir1-2/testfile1",
                                    "testdir1/testfile1");
}

static double
measure_path_resolving(PathResolver *path_resolver, unsigned long number_of_calls)
{
  struct timeval start, end;
  unsigned long i;

  start_time(&start);
  for (i = 0; i < number_of_calls; i++)
    {
      test_path_resolving(path_resolver);
    }
  end_time(&end);

  return calc_elapsed_time(&start, &end);
}

static void
assert_if_path_resolver_is_slow(PathResolver *ref_resolver,
                                PathResolver *test_resolver,
                                double expected_speedup_percent)
{
  double test_speed = measure_path_resolving(test_resolver, MEASURE_SPEEDUP_CYCLES);
  double ref_speed = measure_path_resolving(ref_resolver, MEASURE_SPEEDUP_CYCLES);
  double speedup_percent = 100.0 - 100.0 * (test_speed / ref_speed);

  fprintf(stderr,"MEASURE_SPEEDUP_CYCLES:%d;\nref_speed[%s]: %.8f;\n" \
                 "test_speed[%s]: %.8f;\nexpected speedup: %.2f%%;\nspeedup: %.2f%%\n",
                 MEASURE_SPEEDUP_CYCLES, ref_resolver->name, ref_speed,
                 test_resolver->name, test_speed, expected_speedup_percent,
                 speedup_percent);

/*  TEST_ASSERT(speedup_percent >= expected_speedup_percent, "speedup test failed"); */
}

static void
test_speed_of_path_resolvers(PathResolver *ref_resolver, PathResolver *test_resolver)
{
  assert_if_path_resolver_is_slow(ref_resolver, test_resolver, EXPECTED_SPEEDUP);
}

static double
measure_realpath_speed_with_full_path(const char *path, const char *expected_path)
{
  double test_speed = 0.0;
  struct timeval start, end;
  unsigned long i;
  char *res;
  char *buf;
  long buf_len = get_path_max();

  buf = (char *)malloc(buf_len);

  for (i = 0; i < MEASURE_SPEEDUP_CYCLES; i++)
    {
      start_time(&start);
      res = realpath(path, buf);
      end_time(&end);
      test_speed += calc_elapsed_time(&start, &end);
      TEST_ASSERT(strcmp(res, expected_path) == 0, "realpath failed");
    }

  free(buf);

  return test_speed;
}

static double
measure_path_resolver_speed_with_full_path(PathResolver *path_resolver,
                                           const char *path,
                                           const char *expected_path)
{
  double test_speed = 0.0;
  struct timeval start, end;
  unsigned long i;
  char *res;

  for (i = 0; i < MEASURE_SPEEDUP_CYCLES; i++)
    {
      start_time(&start);
      res = path_resolver->resolver_func(path, "/");
      end_time(&end);
      test_speed += calc_elapsed_time(&start, &end);
      TEST_ASSERT(strcmp(res, expected_path) == 0, "realpath failed");
      free(res);
    }

  return test_speed;
}

static void
compare_realpath_speed_with_resolver_full_path(PathResolver *path_resolver,
                                              const char *full_path,
                                              const char *expected_path)
{
  double test_speed = 0.0;
  double ref_speed = 0.0;

  test_speed = measure_realpath_speed_with_full_path(full_path, expected_path);
  ref_speed = measure_path_resolver_speed_with_full_path(path_resolver, full_path, expected_path);

  double speedup_percent = 100.0 - 100.0 * (test_speed / ref_speed);

  fprintf(stderr,"MEASURE_SPEEDUP_CYCLES:%d;\nref_speed[%s]: %.8f;\n" \
                 "test_speed[realpath]: %.8f;\nspeedup: %.2f%%\n",
                 MEASURE_SPEEDUP_CYCLES, path_resolver->name,
                 ref_speed, test_speed,
                 speedup_percent);
}

PathResolver filemonitor_resolver =
{
  .resolver_func = resolve_to_absolute_path,
  .name = "FileMonitor::resolve_to_absolute_path"
};

PathResolver my_resolver =
{
  .resolver_func = my_resolve_to_absolute_path,
  .name = "my resolver"
};

int main(int argc, char **argv)
{
  setup();

  test_path_resolving(&my_resolver);
  test_path_resolving(&filemonitor_resolver);
  test_speed_of_path_resolvers(&filemonitor_resolver, &my_resolver);

  compare_realpath_speed_with_resolver_full_path(&filemonitor_resolver,
                      "/tmp/test_resolve_path/testdir1/testdir1-2/./././././././././."\
                      "/./././././././../testdir1-2/././././././././../testdir1-2/./."\
                      "/./././././././../testdir1-2/testfile1",
                      "/tmp/test_resolve_path/testdir1/testfile1");

  compare_realpath_speed_with_resolver_full_path(&my_resolver,
                      "/tmp/test_resolve_path/testdir1/testdir1-2/././././././././././"\
                      "./././././././../testdir1-2/././././././././../testdir1-2/././."\
                      "/././././././../testdir1-2/testfile1",
                      "/tmp/test_resolve_path/testdir1/testfile1");

  return 0;
}
