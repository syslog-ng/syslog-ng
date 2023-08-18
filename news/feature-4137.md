`http()`: Added compression ability for use with metered egress/ingress

The new features can be accessed with the following options:
 * `accept-encoding()` for requesting the compression of HTTP responses form the server. ( These are currently not used by syslog-ng, but they still contribute to network traffic.) The available options are `identity` (for no compression), `gzip` or `deflate` between quotation marks. If you want the driver to accept multiple compression types, you can list them separated by commas inside the quotation mark, or write `all`, if you want to enable all available compression types.
 * `content-compression()` for compressing messages sent by syslog-ng. The available options are `identity`(again, for no compression), `gzip`, or `deflate` between quotation marks.
 
 Below you can see a configuration example:
 
```
destination d_http_compressed{
	http(url("127.0.0.1:80"), content-compression("deflate"), accept-encoding("all"));
};
```

