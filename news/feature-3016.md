`python`: allow specifying persist name from python.

From now on users can specify a persist name template from python code.

```
@staticmethod
def generate_persist_name(options):
    return options["file_name"]
```

Currently, in python destination, persist name is generated from the python class name. This can cause inconvenience if the same python class is planned to be used multiple times. For example if someone writes a PythonFileDestination, then the persist name would be python(PythonFileDestination). If a user wants to track many files, that causes persist collision, that needs to be resolved manually in configuration. After this change, module developers can specify a persist name template, that can be generate persist name uniquely (file name can be in included during generation)... Hence there is no need for users to manually resolve persist name collision. Similar example works with python source and fetcher too.

Persist name from config takes precedence over `generate_persist_name`.

Persist name is exposed through `self.persist_name`.
