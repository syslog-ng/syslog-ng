'example-message-generator' : add support for values(name1 => value1, name2 => value2,..) syntax.
The value part follows template syntax.

Example
@version: 3.27
log {
  source { example-msg-generator(template("message parameter")
                                 num(10)
                                 values("PROGRAM" => "program-name"
                                        "current-second" => "$C_SEC"
                                ));
         };
  destination { file(/dev/stdout template("$(format-json --scope all-nv-pairs)\n")); };
};
