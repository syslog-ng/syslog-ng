`http()` and other threaded destinations: fix $SEQNUM processing so that
only local messages get an associated $SEQNUM, just like normal
syslog()-like destinations.  This avoids a [meta sequenceId="XXX"] SD-PARAM
being added to $SDATA for non-local messages.
