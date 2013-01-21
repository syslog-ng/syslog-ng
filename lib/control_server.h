#ifndef CONTROL_SERVER_H
#define CONTROL_SERVER_H 1

#include "syslog-ng.h"
#include <stdio.h>

#define MAX_CONTROL_LINE_LENGTH 4096

typedef struct _Commands
{
  const gchar *command;
  const gchar *description;
  GString *(*func)(GString *command);
} Commands;

typedef struct _ControlServer ControlServer;
typedef struct _ControlConnection ControlConnection;

struct _ControlConnection
{
  GString *input_buffer;
  GString *output_buffer;
  gsize pos;
  ControlServer *server;
  int (*read)(ControlConnection *self, gpointer buffer, gsize size);
  int (*write)(ControlConnection *self, gpointer buffer, gsize size);
  void (*handle_input)(gpointer s);
  void (*handle_output)(gpointer s);
  void (*free_fn)(ControlConnection *self);
};

struct _ControlServer {
  gchar *control_socket_name;
  Commands *commands;
  void (*free_fn)(ControlServer *self);
};


ControlServer *control_server_new(const gchar *path, Commands *commands);
void control_server_start(ControlServer *self);
void control_server_free(ControlServer *self);
void control_server_init_instance(ControlServer *self, const gchar *path, Commands *commands);

#ifdef _WIN32
ControlConnection *control_connection_new(ControlServer *server, HANDLE handle);
#else
ControlConnection *control_connection_new(ControlServer *server, gint fd);
#endif

void control_connection_init_instance(ControlConnection *self, ControlServer *server);

void control_connection_start_watches(ControlConnection *self);
void control_connection_update_watches(ControlConnection *self);
void control_connection_stop_watches(ControlConnection *self);

#endif
