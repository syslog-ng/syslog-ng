`http`: define HTTP signals

This changeset defines the Signal interface for HTTP - with one signal at this time.

What's in the changeset?

* List ADT (abstract data type for list implementations) added to lib.
  * Interface:
    * append
    * foreach
    * is_empty
    * remove_all

Implement the List ADT in http module with `struct curl_slist` for storing the headers.

* HTTP signal(s):
Currently only one signal is added, `header_request`.

Note, that the license for `http-signals.h` is *LGPL* .

