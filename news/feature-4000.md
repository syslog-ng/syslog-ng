`network()`, `syslog()` sources and destinations: added new TLS options `sigalgs()` and `client-sigalgs()`

They can be used to restrict which signature/hash pairs can be used in digital signatures.
It sets the "signature_algorithms" extension specified in RFC5246 and RFC8446.

Example configuration:

```
destination {
    network("test.host" port(4444) transport(tls)
        tls(
            pkcs12-file("/home/anno/Work/syslog-ng/tls/test.host.p12")
            peer-verify(yes)
            sigalgs("RSA-PSS+SHA256:ed25519")
        )
    );
};
```
