#ifndef AFSTREAMS_H_INCLUDED
#define AFSTREAMS_H_INCLUDED

#include "driver.h"

LogDriver *afstreams_sd_new(gchar *filename);
void afstreams_sd_set_sundoor(LogDriver *self, gchar *filename);

#endif
