`java()/python() destinations`: the $SEQNUM macro (and "seqnum" attribute in
Python) was erroneously for both local and non-local logs, while it should
have had a value only in case of local logs to match RFC5424 behavior
(section 7.3.1).  This bug is now fixed, but that means that all non-local
logs will have $SEQNUM set to zero from this version on, e.g.  the $SEQNUM
macro would expand to an string, to match the syslog() driver behaviour.
