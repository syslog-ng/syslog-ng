network sources: fix TLS connection closure

RFC 5425 specifies that once the transport receiver gets `close_notify` from the
transport sender, it MUST reply with a `close_notify`.

The `close_notify` alert is now sent back correctly in case of TLS network sources.
