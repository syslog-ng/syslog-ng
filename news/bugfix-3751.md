Fix network sources on NetBSD
 

On NetBSD, TCP-based network sources closed their listeners shortly after startup due to a non-portable TCP keepalive setting. This has been fixed.
