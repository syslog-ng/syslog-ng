`LogThreadedSourceDriver` and `Fetcher`: implement source-side batching
support on the input path by assigning a thread_id to dynamically spawned
input threads (e.g. those spawned by LogThreadedSourceDriver) too.  To
actually improve performance the source driver should disable automatic
closing of batches by setting `auto_close_batches` to FALSE and calling
log_threaded_source_close_batch() explicitly.
