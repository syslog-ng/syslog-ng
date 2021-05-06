`mongodb()`: template support for the `collection()` option

The `collection()` option of the MongoDB destination driver now accepts
templates, for example:

```
destination {
  mongodb(
    uri("mongodb://host/syslog?wtimeoutMS=10000&socketTimeoutMS=10000&connectTimeoutMS=10000&serverSelectionTimeoutMS=5000")
    collection("${HOST}_messages")
    workers(8)
  );
};
```
