`$(format-json)`: fix a bug in the --key-delimiter option introduced in
3.38, which causes the generated JSON to contain multiple values for the
same key in case the key in question contains a nested object and
key-delimiter specified is not the dot character.
