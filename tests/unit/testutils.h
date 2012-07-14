#ifndef LIBTEST_H_INCLUDED
#define LIBTEST_H_INCLUDED

#include <glib.h>
#include <sys/time.h>
#include <stdlib.h>

#define PRETTY_STRING_FORMAT "%s%s%s"
#define PRETTY_STRING(str) ((str) ? "'" : "<"), ((str) ? (str) : "NULL"), ((str) ? "'" : ">")

#define PRETTY_NSTRING_FORMAT "%s%.*s%s"
#define PRETTY_NSTRING(str, len) ((str) ? "'" : "<"), len, ((str) ? (str) : "NULL"), ((str) ? "'" : ">")

#define array_count_guint32(array) ((array != NULL) ? ((guint32)(sizeof(array) / sizeof(guint32))) : 0)

#define gboolean_to_string(value) (value ? "TRUE" : "FALSE")

#define ASSERTION_ERROR(message) "%s:%d/%s\n  #       %s", \
                                 __FILE__, __LINE__, __FUNCTION__, ((message) ? (message) : "")

void start_stopwatch();
void stop_stopwatch_and_display_result(gchar *message_template, ...);
gchar **fill_string_array(gint number_of_elements, ...);

gboolean assert_gint64_non_fatal(gint64 actual, gint64 expected, const gchar *error_message, ...);
gboolean assert_guint64_non_fatal(guint64 actual, guint64 expected, const gchar *error_message, ...);
gboolean assert_nstring_non_fatal(const gchar *actual, gint actual_len, const gchar *expected, gint expected_len, const gchar *error_message, ...);
gboolean assert_guint32_array_non_fatal(guint32 *actual, guint32 actual_length, guint32 *expected, guint32 expected_length, const gchar *error_message, ...);
gboolean assert_gchar_array_non_fatal(gchar *actual, guint32 actual_length, gchar *expected, guint32 expected_length, const gchar *error_message, ...);
gboolean assert_string_array_non_fatal(gchar **actual, guint32 actual_length, gchar **expected, guint32 expected_length, const gchar *error_message, ...);
gboolean assert_gboolean_non_fatal(gboolean actual, gboolean expected, const gchar *error_message, ...);
gboolean assert_null_non_fatal(void *pointer, const gchar *error_message, ...);
gboolean assert_not_null_non_fatal(void *pointer, const gchar *error_message, ...);
gboolean assert_no_error_non_fatal(GError *error, const gchar *error_message, ...);
gboolean assert_guint32_set_non_fatal(guint32 *actual, guint32 actual_length, guint32 *expected, guint32 expected_length, const gchar *error_message, ...);
gboolean assert_gpointer_non_fatal(gpointer actual, gpointer expected, const gchar *error_message, ...);

#define assert_gint64(actual, expected, error_message, ...) (assert_gint64_non_fatal(actual, expected, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))
#define assert_guint64(actual, expected, error_message, ...) (assert_guint64_non_fatal(actual, expected, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))

#define assert_gint(actual, expected, error_message, ...) (assert_gint64((gint64)actual, (gint64)expected, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))
#define assert_guint(actual, expected, error_message, ...) (assert_guint64((guint64)actual, (guint64)expected, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))

#define assert_gint32(actual, expected, error_message, ...) (assert_gint64((gint64)actual, (gint64)expected, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))
#define assert_guint32(actual, expected, error_message, ...) (assert_guint64((guint64)actual, (guint64)expected, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))

#define assert_string(actual, expected, error_message, ...) (assert_nstring_non_fatal(actual, -1, expected, -1, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))
#define assert_nstring(actual, actual_len, expected, expected_len, error_message, ...) (assert_nstring_non_fatal(actual, actual_len, expected, expected_len, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))

#define assert_guint32_array(actual, actual_length, expected, expected_length, error_message, ...) ( \
    assert_guint32_array_non_fatal(actual, actual_length, expected, expected_length, error_message, ##__VA_ARGS__)\
 ? 1 : (exit(1),0))

#define assert_gchar_array(actual, actual_length, expected, expected_length, error_message, ...) ( \
    assert_gchar_array_non_fatal(actual, actual_length, expected, expected_length, error_message, ##__VA_ARGS__)\
 ? 1 : (exit(1),0))

#define assert_string_array(actual, actual_length, expected, expected_length, error_message, ...) ( \
    assert_string_array_non_fatal(actual, actual_length, expected, expected_length, error_message, ##__VA_ARGS__)\
 ? 1 : (exit(1),0))

#define assert_gboolean(actual, expected, error_message, ...) (assert_gboolean_non_fatal(actual, expected, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))
#define assert_true(value, error_message, ...) (assert_gboolean(value, TRUE, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))
#define assert_false(value, error_message, ...) (assert_gboolean(value, FALSE, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))

#define assert_null(pointer, error_message, ...) (assert_null_non_fatal((void *)pointer, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))
#define assert_not_null(pointer, error_message, ...) (assert_not_null_non_fatal((void *)pointer, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))

#define assert_no_error(error, error_message, ...) (assert_no_error_non_fatal(error, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))
#define assert_guint32_set(actual, actual_length, expected, expected_length, error_message, ...) (assert_guint32_set_non_fatal(actual, actual_length, expected, expected_length, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))

#define assert_gpointer(actual, expected, error_message, ...) (assert_gpointer_non_fatal(actual, expected, error_message, ##__VA_ARGS__) ? 1 : (exit(1),0))

#endif
