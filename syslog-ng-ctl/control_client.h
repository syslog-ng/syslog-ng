#ifndef CONTROL_CLIENT_H
#define CONTROL_CLIENT_H 1

#include "syslog-ng.h"

typedef struct _ControlClient ControlClient;

ControlClient *control_client_new(const gchar *path);
gboolean control_client_connect(ControlClient *self);
gint control_client_send_command(ControlClient *self, const gchar *cmd);
GString* control_client_read_reply(ControlClient *self);
void control_client_free(ControlClient *self);

#endif
