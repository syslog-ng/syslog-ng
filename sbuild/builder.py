from AutotoolsBuilder import AutotoolsBuilder
import os

def get_builder():
    builder = AutotoolsBuilder(get_default_config_opts())
    os.environ["PKG_CONFIG_PATH"] = "%s/lib/pkgconfig"%(builder.install_dir)
    return builder


def get_default_config_opts():
    try:
        import configure_opts
    except ImportError:
        return None
    else:
        return configure_opts.default_opts
