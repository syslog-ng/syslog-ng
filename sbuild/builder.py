from AutotoolsBuilder import AutotoolsBuilder
import os

class SyslogNGCoreBuilder(AutotoolsBuilder):

    def environment_override(self):
        os.environ["PKG_CONFIG_PATH"] = "%s/lib/pkgconfig"%(self.install_dir)
        os.environ["PATH"] += ":%s/bin"%(self.install_dir)
        print "PATH = %s"%(os.environ["PATH"])

def get_builder():
    builder = SyslogNGCoreBuilder(get_default_config_opts())
    return builder


def get_default_config_opts():
    try:
        import configure_opts
    except ImportError:
        return None
    else:
        return configure_opts.default_opts
