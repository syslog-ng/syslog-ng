#ifndef CHILDREN_H_INCLUDED
#define CHILDREN_H_INCLUDED

#include "syslog-ng.h"
#include <sys/types.h>

void child_manager_register(pid_t pid, void (*callback)(pid_t, int, gpointer), gpointer user_data);
void child_manager_sigchild(pid_t pid, int status);

void child_manager_init(void);
void child_manager_deinit(void);

#endif
