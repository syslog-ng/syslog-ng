`afmongodb`: Bulk MongoDB insert is added via the following options

- `bulk`  (**yes**/no)  turns on/off [bulk insert ](http://mongoc.org/libmongoc/current/bulk.html)usage, `no` forces the old behavior (each log is inserted one by one into the MongoDB)
- `bulk_unordered` (yes/**no**)  turns on/off [unordered MongoDB bulk operations](http://mongoc.org/libmongoc/current/bulk.html#unordered-bulk-write-operations)
- `bulk_bypass_validation`  (yes/**no**)  turns on/off [MongoDB bulk operations validation](http://mongoc.org/libmongoc/1.23.3/bulk.html#bulk-operation-bypassing-document-validation)
- `write_concern` (unacked/**acked**/majority/n > 0)  sets [write concern mode of the MongoDB operations](http://mongoc.org/libmongoc/1.23.3/bulk.html#bulk-operation-write-concerns), both bulk and single

NOTE: Bulk sending is only efficient if the used collection is constant (e.g. not using templates) or the used template does not lead to too many collections switching within a reasonable time range.
