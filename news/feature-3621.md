`mongodb()`: add `workers()` support (multi-threaded connection pooling)

The MongoDB driver now supports the `workers()` option, which specifies the
number of parallel workers to be used.
Workers are based on the connection pooling feature of the MongoDB C library.

This increases the throughput of the MongoDB destination driver.

Example:

```
destination {
  mongodb(
    uri("mongodb://hostA,hostB/syslog?replicaSet=my_rs&wtimeoutMS=10000&socketTimeoutMS=10000&connectTimeoutMS=10000&serverSelectionTimeoutMS=5000")
    collection("messages")
    workers(8)
  );
};
```
