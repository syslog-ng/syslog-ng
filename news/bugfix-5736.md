`format-json`: Fixed nested `$(format-json)` so a key is no longer placed inside an object whose name is only a
string prefix of it, for example `hostname` is no longer absorbed into a `host.*` object.
