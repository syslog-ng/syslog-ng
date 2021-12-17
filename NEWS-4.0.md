
Implement typing support throughout syslog-ng

* Introduce runtime type information in the LogMessage

  syslog-ng uses a data model where a log message contains an unordered set
  of name-value pairs. The values stored in these name-value pairs are
  usually textual and so syslog-ng has traditionally stored these values in
  text format.

  With the increase of JSON based message sources and destinations, types
  became more important.  If we encounter a message where a name-value pair
  originates from a JSON document and this document contains a numeric data
  member, we may want to reproduce that.

  Sometimes we extract a metric from a log message and we need to send this
  to a consumer, again with the correct type.

  To be able to do this, we added runtime type information to the syslog-ng
  message model: each name-value pair becomes a (name, type, value) triplet.

  We introduced the following types:
    - string: simple textual data, mostly utf8 (but not always)
    - int: an integer representable by a 64 bit signed value
    - double: a double precision floating point number
    - boolean: true or false
    - datetime: Date and Time represented by the milliseconds since epoch
    - list: list of strings
    - json: JSON snippet
    - null: an unset value

  Apart from the syslog-ng core supporting the notion of types, its up to
  the sources, filters, rewrite rules, parsers and destinations that set or
  make use of them, however it makes most sense for the component in
  question.

* Type aware comparisons

  syslog-ng uses filter expressions to make routing decisions and during the
  transformation of messages.  These filter expressions are used in filter
  {} or if {} statements, for example.

  In these expressions, you can use comparison operators, this example for
  instance uses the '>' operator to check for HTTP response codes
  greater-or-equal than 500:

```
     if ("${apache.response}" >= 500) {
     };
```

  Earlier we had two sets of operators, one for numeric (==, !=, <, >) the
  other for string based comparisons (eq, ne, gt, lt).

  The separate operators were cumbersome to use, people often forgot (me
  included) which operator is the right one for a specific case.

  Typing allows us to do the right thing in most cases automatically and a
  syntax that allows the user to override the automatic decisions in the
  rare case.

  With that, starting with 4.0, the old-numeric operators have been
  converted to be type-aware operators. It would compare as strings if both
  side of the comparisons are strings. It would compare numerically if at
  least one side is numeric. A great deal of inspiration was taken from
  JavaScript, which was considered to be a good model, since the problem
  space is similar.

  See this blog post for more details:
    https://syslog-ng-future.blog/syslog-ng-4-progress-3-38-1-release/

* Capture type information from JSON

  When using json-parser(), syslog-ng converts all members of a JSON object
  to syslog-ng name-value pairs.  Pre-type support these name-value pairs
  are all stored as strings, any type information originally present in the
  JSON object are lost.  This means that if you regenerate the JSON from the
  name-value pairs using $(format-json), all numbers, booleans, etc become
  strings in the output.

  There is a feature in syslog-ng that aleviate the loss of types, the
  concept that we call "type-hints". Type-hints tell $(format-json) to use a
  specific type on output, but this type conversion needed to be explicit in
  the configuration.

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

  The new feature introduced with typing is that syslog-ng would
  automatically store the JSON type information as a syslog-ng type, thus it
  will transparently carry over types from inputs to output, without having
  to be explicit about them.

* Typing support for various components in syslog-ng

  * type aware comparisons in filter expressions: as detailed above, the
    previously numeric operators become type aware and the exact comparison
    performed will be based on types associated with the values that we
    compare.

  * json-parser() and $(format-json): JSON support is massively improved
    with the introduction of types.  For one: type information is retained
    across input parsing->transformation->output formatting.  JSON lists
    (arrays) are now supported and are converted to syslog-ng lists so they
    can be manipulated using the $(list-*) template functions.  There are
    other important improvements in how we support JSON.

  * set(), groupset(): in any case where we allow the use of templates,
    support for type-casting was added and the type information is properly
    promoted.

  * db-parser() type support: db-parser() gets support for type casts,
    <value> assignments within db-parser() rules can associate types with
    values using the type-casting syntax, e.g.  <value
    name=”foobar”>int32($PID)</value>.  The “int32” is a type-cast that
    associates $foobar with an integer type.  db-parser()’s internal parsers
    (e.g.  @NUMBER@) will also associated type information with a name-value
    pair automatically.

  * add-contextual-data() type support: any new name-value pair that is
    populated using add-contextual-data() will propagate type information,
    similarly to db-parser().

  * map-value-pairs() type support: propagate type information

  * SQL type support: the sql() driver gained support for types, so that
    columns with specific types will be stored as those types.

  * template type support: templates can now be casted explicitly to a
    specific type, but they also propagate type information from
    macros/template functions and values in the template string

  * value-pairs type support: value-pairs form the backbone of specifying a
    set of name-value pairs and associated transformations to generate JSON
    or a key-value pair format.  It also gained support for types, the
    existing type-hinting feature that was already part of value-pairs was
    adapted and expanded to other parts of syslog-ng.

  * on-disk serialized formats (e.g.  disk buffer/logstore): we remain
    compatible with messages serialized with an earlier version of
    syslog-ng, and the format we choose remains compatible for “downgrades”
    as well.  E.g.  even if a new version of syslog-ng serialized a message,
    the old syslog-ng and associated tools will be able to read it (sans
    type information of course)

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
