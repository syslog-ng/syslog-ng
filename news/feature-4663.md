`http()`: Added support for using templates in the `url()` option.

In syslog-ng a template can only be resolved on a single message, as the same
template might have different resolutions on different messages. A http batch
consists of multiple messages, so it is not trivial to decide which message should
be used for the resolution.

When batching is enabled and multiple workers are configured it is important to
only batch messages which generate identical URLs. In this scenario one must set
the `worker-partition-key()` option with a template that contains all the templates
used in the `url()` option, otherwise messages will be mixed.

For security reasons, all the templated contents in the `url()` option are getting
URL encoded automatically. Also the following parts of the url cannot be templated:
  * scheme
  * host
  * port
  * user
  * password
