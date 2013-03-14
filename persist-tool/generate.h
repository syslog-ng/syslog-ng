#ifndef GENERATE_H
#define GENERATE_H 1

#include "persist-tool.h"
#include "syslog-ng.h"
#include "mainloop.h"
#include "persist-state.h"
#include "plugin.h"
#include "state.h"
#include "cfg.h"

gboolean force_generate;
gchar *config_file_generate;
gchar *generate_output_dir;

gint generate_main(int argc, char *argv[]);

#endif
