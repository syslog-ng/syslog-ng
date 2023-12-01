#ifndef FILTERX_CONFIG_H_INCLUDED
#define FILTERX_CONFIG_H_INCLUDED 1

#include "module-config.h"
#include "filterx/filterx-object.h"

typedef struct _FilterXConfig
{
  ModuleConfig super;
  GPtrArray *frozen_objects;
} FilterXConfig;

FilterXConfig *filterx_config_get(GlobalConfig *cfg);
FilterXObject *filterx_config_freeze_object(GlobalConfig *cfg, FilterXObject *object);

#endif
