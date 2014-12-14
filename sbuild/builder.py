from AutotoolsBuilder import AutotoolsBuilder
import os
import Utils
import subprocess

class SyslogNGCoreBuilder(AutotoolsBuilder):
    def get_source_revision(self):
        branch_cmd = "git branch | grep '^\* ' | cut -c3-"
        branch = subprocess.check_output(branch_cmd, shell=True, close_fds=False, cwd=self.source_dir).strip()
        remote_cmd = "git rev-parse --abbrev-ref --symbolic-full-name @{u} | cut -f1 -d/"
        remote = subprocess.check_output(remote_cmd, shell=True, close_fds=False, cwd=self.source_dir).strip()
        remote_url_cmd = "git remote -v | grep \"^%s\" | head -1 | awk '{print $2}'"%remote
        remote_url = subprocess.check_output(remote_url_cmd, shell=True, close_fds=False, cwd=self.source_dir).strip()
        sha_cmd = "git rev-list --max-count=1 HEAD"
        sha = subprocess.check_output(sha_cmd, shell=True, close_fds=False, cwd=self.source_dir).strip()
        return "%s#%s#%s"%(remote_url, branch, sha)

    def environment_override(self):
        revision = self.get_source_revision()
        self._add_extra_env(
         {"PKG_CONFIG_PATH" : "%s/lib/pkgconfig"%(self.install_dir),
          "PATH": os.environ['PATH'] + ":%s/bin"%(self.install_dir),
          "SOURCE_REVISION" : revision,
         })

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
