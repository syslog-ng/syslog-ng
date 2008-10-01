#ifndef GPROCESS_H_INCLUDED
#define GPROCESS_H_INCLUDED

#include "syslog-ng.h"

#include <sys/types.h>

#if ENABLE_LINUX_CAPS
#  include <sys/capability.h>
#endif

typedef enum
{
  G_PM_FOREGROUND,
  G_PM_BACKGROUND,
  G_PM_SAFE_BACKGROUND,
} GProcessMode;

#if ENABLE_LINUX_CAPS

gboolean g_process_cap_modify(int capability, int onoff);
cap_t g_process_cap_save(void);
void g_process_cap_restore(cap_t r);

#else

typedef gpointer cap_t;

#define g_process_cap_modify(cap, onoff)
#define g_process_cap_save() NULL
#define g_process_cap_restore(cap)

#endif

void g_process_message(const gchar *fmt, ...);

void g_process_set_mode(GProcessMode mode);
void g_process_set_name(const gchar *name);
void g_process_set_user(const gchar *user);
void g_process_set_group(const gchar *group);
void g_process_set_chroot(const gchar *chroot);
void g_process_set_pidfile(const gchar *pidfile);
void g_process_set_pidfile_dir(const gchar *pidfile_dir);
void g_process_set_working_dir(const gchar *cwd);
void g_process_set_caps(const gchar *caps);
void g_process_set_argv_space(gint argc, gchar **argv);
void g_process_set_use_fdlimit(gboolean use);
void g_process_set_check(gint check_period, gboolean (*check_fn)(void));

void g_process_start(void);
void g_process_startup_failed(guint ret_num, gboolean may_exit);
void g_process_startup_ok(void);
void g_process_finish(void);

void g_process_add_option_group(GOptionContext *ctx);


#endif
