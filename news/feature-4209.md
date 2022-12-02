`network`/`syslog`/`tls` context options: SSL_CONF_cmd support

SSL_CONF_cmd TLS configuration support for `network()` and `syslog()` driver has been added.

OpenSSL offers an alternative, software-independent configuration mechanism through the SSL_CONF_cmd
interface to support a common solution for setting the so many various SSL_CTX and SSL options that
can be set earlier via multiple, separated openssl function calls only.
This update implements that similar to the mod_ssl in Apache.

IMPORTANT:
The newly introduced `openssl-conf-cmds` always has the highest priority, its content parsed last,
so it will override any other options can be found in the `tls()` section, does not matter if they
appear before or after `openssl-conf-cmds`.

Regarding to the other `tls()` options.
As described in the SSL_CONF_cmd documentation, the order of operations is significant and executed in
top-down order, basically it means if there are multiple occurrences of setting the same option then the 'last wins'.
This is also true for options that can be set multiple ways (e.g. used cipher suites and/or protocols).

Example config:
```
source source_name {
    network (
        ip(0.0.0.0)
        port(6666)
        transport("tls")
        tls(
            ca-dir("/etc/ca.d")
            key-file("/etc/cert.d/serverkey.pem")
            cert-file("/etc/cert.d/servercert.pem")
            peer-verify(yes)

            openssl-conf-cmds(
                # For system wide available cipher suites use: /usr/bin/openssl ciphers -v
                # For formatting rules see: https://www.openssl.org/docs/man1.1.1/man3/SSL_CONF_cmd.html
                # For quick and dirty testing try: https://github.com/rbsec/sslscan
                #
                "CipherString" => "ECDHE-RSA-AES128-SHA",                                   # TLSv1.2 and bellow
                "CipherSuites" => "TLS_CHACHA20_POLY1305_SHA256:TLS_AES_256_GCM_SHA384",    # TLSv1.3+ (OpenSSl 1.1.1+)

                "Options" => "PrioritizeChaCha",
                "Protocol" => "-ALL,TLSv1.3",
            )
        )
    );
};



```
