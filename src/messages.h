#ifndef MESSAGES_H_INCLUDED
#define MESSAGES_H_INCLUDED

#include <syslog-ng.h>
#include <evtlog.h>

extern int debug_flag;
extern int verbose_flag;
extern int msg_pipe[2];

#define msg_fatal(desc, tag1, tags...) msg_event(EVT_PRI_CRIT, desc, tag1, ##tags )
#define msg_error(desc, tag1, tags...) msg_event(EVT_PRI_ERR, desc, tag1, ##tags )
#define msg_notice(desc, tag1, tags...) msg_event(EVT_PRI_NOTICE, desc, tag1, ##tags )

#define msg_verbose(desc, tag1, tags...) \
	do { \
	  if (verbose_flag) \
	    msg_notice(desc, tag1, ##tags ); \
	} while (0)
	
#define msg_debug(desc, tag1, tags...) \
	do { \
	  if (debug_flag) \
	    msg_event(EVT_PRI_DEBUG, desc, tag1, ##tags ); \
	} while (0)

void msg_event(gint prio, const char *desc, EVTTAG *tag1, ...);

gboolean msg_init(int use_stderr);
void msg_deinit();

#endif
