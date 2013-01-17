#ifndef CONTROL_SERVER_H
#define CONTROL_SERVER_H 1

#include "syslog-ng.h"

#define MAX_CONTROL_LINE_LENGTH 4096

typedef struct _ControlServer ControlServer;

typedef GString *(*COMMAND_HANDLER)(GString *command);

ControlServer *control_server_new(const gchar *path);
gboolean control_server_start(ControlServer *self);
void control_server_register_command_handler(ControlServer *self,const gchar *cmd, COMMAND_HANDLER handler);
void control_server_free(ControlServer *self);

#endif
