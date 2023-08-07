`python`: Added a convenience function to register jinja templated config generators.

Example usage:
```
python {
import syslogng

syslogng.register_jinja_config_generator(context="destination", name="my_dest", template=r"""

file({{ args.path }});

""")
};

destination d_my_dest{
  my-dest(path("/dev/stdout"));
};
```
