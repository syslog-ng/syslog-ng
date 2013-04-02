#ifndef GPO_UTILS_H
#define GPO_UTILS_H 1

#include <glib.h>

typedef struct _GpoStruct
{
  gchar *display_name;
  gchar *full_file_name;
  gchar *registry_root_key;
} GpoStruct;

GHashTable *GPODisover(const gboolean write_gpo_tmp_regkeys);
int upgrade_gpo_config(const gchar *gpoid);
int upgrade_binary_reg_file(const gchar *filename);
int upgrade_registry_path(const gchar *path);

#endif
