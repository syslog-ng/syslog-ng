/*
 * COPYRIGHTHERE
 */

#ifndef FILEMONITOR_H_INCLUDED
#define FILEMONITOR_H_INCLUDED
#include "syslog-ng.h"

#define END_OF_LIST (gchar*)file_monitor_set_file_callback

typedef enum { ACTION_NONE, ACTION_CREATED, ACTION_MODIFIED, ACTION_DELETED} FileActionType;

typedef gboolean (*FileMonitorCallbackFunc)(const gchar *filename, gpointer user_data, FileActionType action_type);

typedef enum { MONITOR_NONE, MONITOR_POLL, MONITOR_INOTIFY, MONITOR_WINDOWS } MonitorType;

typedef struct  _FileMonitor
{
  GSList *sources;
  GPatternSpec *compiled_pattern;
  FileMonitorCallbackFunc file_callback;
  GSourceFunc destroy_callback;
  gpointer user_data;
  gboolean recursion;
  MonitorType monitor_type;
  gint poll_freq;
} FileMonitor;

FileMonitor *file_monitor_new();
void file_monitor_free(FileMonitor *self);
void file_monitor_set_file_callback(FileMonitor *self, FileMonitorCallbackFunc file_callback, gpointer user_data);
void file_monitor_set_destroy_callback(FileMonitor *self, GSourceFunc destroy_callback, gpointer user_data);
void file_monitor_set_poll_freq(FileMonitor *self, gint poll_freq);
gboolean file_monitor_use_timeout(FileMonitor *self);
gboolean file_monitor_watch_directory(FileMonitor *self, const gchar *filename);
gboolean file_monitor_stop(FileMonitor *self);
void file_monitor_deinit(FileMonitor *self);

#endif
