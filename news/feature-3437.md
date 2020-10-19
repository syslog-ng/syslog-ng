transport: add proxy-protocol support

http://www.haproxy.org/download/1.8/doc/proxy-protocol.txt

<details>
  <summary>Example config, click to expand!</summary>

```
@version: 3.29

source s_tcp_pp {
    network(
        port(7777)
#        transport("proxied-tcp")
        transport("proxied-tls")
        tls(
             key-file("/openssl/certs/certs/server/server.rsa")
             cert-file("openssl/certs/certs/server/server.crt")
             ca-dir("/openssl/certs/certs/CA")
#             peer-verify("optional-untrusted")
             peer-verify("required-trusted")
         )
    );
};

destination d_file {
    file("/var/log/pp.log" template("$(format-json --scope nv-pairs)\n"));
};

log {
    source(s_tcp_pp);
    destination(d_file);
};

```

</details>


