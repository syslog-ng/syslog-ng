#ifndef UPGRADE_LOG_H
#define UPGRADE_LOG_H 1

#include <glib.h>

typedef struct _UpgradeLog UpgradeLog;

UpgradeLog *upgrade_log_new(gchar *filename);
void upgrade_log_error(UpgradeLog *self, gchar *msg_format, ...);
void upgrade_log_warning(UpgradeLog *self, gchar *msg_format, ...);
void upgrade_log_info(UpgradeLog *self, gchar *msg_format, ...);

void upgrade_log_free(UpgradeLog *self);

#endif
