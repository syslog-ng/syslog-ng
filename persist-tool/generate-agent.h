#ifndef GENERATE_AGENT_H
#define GENERATE_AGENT_H 1

#include "persist-tool.h"
#include "syslog-ng.h"
#include "mainloop.h"
#include "persist-state.h"
#include "plugin.h"
#include "state.h"
#include "cfg.h"

gboolean force_generate_agent;
gchar *generate_agent_output_dir;
gchar *xml_config_file;

gint generate_agent_main(int argc, char *argv[]);

#endif
