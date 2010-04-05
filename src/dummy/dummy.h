#ifndef DUMMY_H_INCLUDED
#define DUMMY_H_INCLUDED

#include "driver.h"

typedef struct
{
  LogDriver super;
  gint opt;
} DummyDestDriver;

DummyDestDriver *dummy_dd_new(void);

#endif
