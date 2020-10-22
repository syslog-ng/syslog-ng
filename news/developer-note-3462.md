Proxy protocol support added to loggen.

Four new options added to loggen to suppport the proxy protocol:
* --proxied : Generate PROXY protocol v1 header
* --proxy-src-ip : Set the source IP for the PROXY protocol v1 header. If not specified a random IP address generated (192.168.1.X).
* --proxy-dst-ip : Set the destination IP for the PROXY protocol v1 header. If not specified a random IP address generated (192.168.1.X).
* --proxy-src-port : Set the source port for the PROXY protocol v1 header. If not specified a random port generated in the range 5000-10000.
* --proxy-dst-port : Set the destination port for the PROXY protocol v1 header. If not specified the port number 514 will be used.
