Fix IPv4 UDP destinations on FreeBSD

UDP-based destinations crashed when receiving the first message on FreeBSD due
to a bug in destination IP extraction logic.
