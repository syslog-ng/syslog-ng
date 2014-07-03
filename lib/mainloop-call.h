#ifndef MAINLOOP_CALL_H
#define MAINLOOP_CALL_H 1

#include "mainloop.h"

gpointer main_loop_call(MainLoopTaskFunc func, gpointer user_data, gboolean wait);

void main_loop_call_thread_init(void);
void main_loop_call_thread_deinit(void);

void main_loop_call_init(void);
void main_loop_call_deinit(void);

#endif
