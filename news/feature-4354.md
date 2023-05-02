`python`: Added support for typed custom options.

This applies for `python` source, `python-fetcher` source, `python` destination,
`python` parser and `python-http-header` inner destination.

Example config:
```
python(
  class("TestClass")
  options(
    "string_option" => "example_string"
    "bool_option" => True  # supported values are: True, False, yes, no
    "integer_option" => 123456789
    "double_option" => 123.456789
    "string_list_option" => ["string1", "string2", "string3"]
    "template_option" => LogTemplate("${example_template}")
  )
);
```

**Breaking change! Previously values were converted to strings if possible, now they are passed
to the python class with their real type. Make sure to follow up these changes
in your python code!**
