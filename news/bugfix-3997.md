fix `threaded(no)` related crash: if threaded mode is disabled for
asynchronous sources and destinations (all syslog-like drivers such as
tcp/udp/syslog/network qualify), a use-after-free condition can happen due
to a reference counting bug in the non-threaded code path.  The
`threaded(yes)` setting has been the default since 3.6.1 so if you are using
a more recent version, you are most probably unaffected.  If you are using
`threaded(no)` a use-after-free condition happens as the connection closes.
The problem is more likely to surface on 32 bit platforms due to pointer
sizes and struct layouts where this causes a NULL pointer dereference.
