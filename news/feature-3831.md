
Implement typing support throughout syslog-ng

* Proper capture of type information from JSON

  When using json-parser(), syslog-ng converts all JSON attributes to
  syslog-ng name-value pairs. Traditionally these name-value pairs are
  strings, so when regenerating a JSON at output, all types are
  automatically converted to strings. The only work around so far were
  "type-hints", which tell $(format-json) to use a specific type on output,
  but this type conversion needed to be explicit in the configuration.

  An example configuration that parses JSON on input and produces a JSON on
  output:

```
log {
    source { ... };
    parser { json-parser(prefix('.json.')); };
    destination { file(... template("$(format-json .json.*)\n")); };
};
```

  To augment the above with type hinting, you could use:

```
log {
    source { ... };
    parser { json-parser(prefix('.json.')); };
    destination { file(... template("$(format-json .json.* .json.value=int64(${.json.value})\n")); };
};
```

  The new feature is that syslog-ng would transparently carry over types
  from inputs to input, so the original configuration would produce an
  integer if the input was integer, without the use of type hints.

* Support for types in all key aspects of syslog-ng

  - name-value pairs
  - macros
  - template expressions
  - set()
  - type-hints or type casts on output
  - $CONTEXT_ID and $_

* Type aware destinations and automatic conversion of data

  - JSON
  - MongoDB
  - Riemann

  not yet:
  - SQL?
  - Python?

  Type conversions:
  - datetime

* Improved support for lists (arrays)

  For syslog-ng, everything is traditionally a string.  A convention was
  started with syslog-ng with syslog-ng 3.10 where a comma separated format
  could be used as a kind of array, using the `$(list-*)` family of template
  functions.

  For example, $(list-head) takes off the first element in a list, while
  $(list-tail) takes the last.  You can index and slice list elements using
  the $(list-slice) and $(list-nth) functions and so on.

  syslog-ng has started to return such lists in various cases, so they can
  be manipulated using these list specific template functions.  These
  include the xml-parser(), or the $(explode) template function, but there
  are others.

  Here is an example that has worked since syslog-ng 3.10:

  ```
    # MSG contains foo:bar:baz
    # - the $(list-head) takes off the first element of a list
    # - the $(explode) expression splits a string at the specified separator, ':' in this case.
    $(list-head $(explode : $MSG))
  ```

  New functions that improve these features:
    - JSON arrays are converted to lists, making it a lot easier to slice
      and extract information from JSON arrays.  Of course $(format-json)
      will take lists and convert them back to arrays.


    - The $* is a new macro, that converts the internal list of match
      variables ($1, $2, $3 and so on) to a list, usable with $(list-*)
      template functions.  These match variables have traditionally been
      filled by regular expressions when a capture group in a regexp
      matches.

    - The set-matches() rewrite operation performs the reverse, it assigns
      the match variables to list elements, making it easier to use list
      elements in template expressions by assigning them to $1, $2, $3 and
      so on.

    - Top-level JSON arrays (e.g.  ones where the incoming JSON data is an
      array and not an object) are now accepted, and the array elements are
      assigned to the match variables.

* Numeric literals in configuration and type aware comparison operators
  - not having to use quotes around expressions
  - numeric expressions
  - type hints

* type aware comparisons

* boolean expressions in templates?
  - if ("$(whatever)") ...

* slight changes in behavior
  - json-parser handling of arrays
  - match counting

* ideas
  - comparison
  - exclude matches from value-pairs by default
  - change value-pairs expression to make it easier to evaluate
    (include/exclude functionality)
  - make value pairs _fast_ (format-json for sure, also json-parser())
  - csv-parser as a template function (a'la explode but via proper CSV
    support)
  - python typing
