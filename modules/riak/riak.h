#ifndef RIAK_H_INCLUDED
#define RIAK_H_INCLUDED

#include "driver.h"

LogDriver *riak_dd_new(GlobalConfig *cfg);

void riak_dd_set_host(LogDriver *d, const char *host);
void riak_dd_set_port(LogDriver *d, int port);
void riak_dd_set_bucket(LogDriver *d, LogTemplate *bucket)
void riak_dd_set_mode(LogDriver *d, char *mode)
void riak_dd_set_type(LogDriver *d, char *type)
void riak_dd_set_key(LogDriver *d, LogTemplate *key)
void riak_dd_set_value(LogDriver *d, LogTemplate *value)

#endif

