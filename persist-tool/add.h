#ifndef ADD_H
#define ADD_H 1

#include "syslog-ng.h"
#include "mainloop.h"
#include "persist-state.h"
#include "plugin.h"
#include "state.h"
#include "cfg.h"
#include "persist-tool.h"

gchar *persist_state_dir;

gint add_main(int argc, char *argv[]);


#endif
