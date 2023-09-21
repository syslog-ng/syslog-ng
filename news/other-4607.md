`vim`: Syntax highlight file is no longer packaged.

vim syntax files where previously installed by the RedHat packages of syslog-ng
(but not the Debian ones). These files where sometime lagging behind, so in order
to provide a more up-to-date experience on all platforms, regardless of the
installation of the syslog-ng package, the vim syntax files have been moved to a
dedicated repository [syslog-ng/vim-syslog-ng](https://github.com/syslog-ng/vim-syslog-ng) that can be used using a plugin manager such as
[vim-plug](https://github.com/junegunn/vim-plug), [vim-pathogen](https://github.com/tpope/vim-pathogen) or [vundle](https://github.com/VundleVim/Vundle.vim).
