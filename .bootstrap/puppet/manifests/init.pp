$PATH='/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games'
$VAGRANT_HOME='/home/vagrant'
$PROJECT_HOME='/vagrant'
$VIRTUALENV="$VAGRANT_HOME/.syslog-ng-virtualenv"

file { 'locale':
  ensure  => present,
  path    => '/etc/environment',
  content => "LANGUAGE=en_US.UTF-8\nLC_ALL=en_US.UTF-8\nPATH=$PATH",
  owner   => root,
  group   => root,
  mode    => '644',
}

file { 'hostname':
  ensure  => present,
  path    => '/etc/hostname',
  content => 'syslog-ng.devenv',
  owner   => root,
  group   => root,
  mode    => '644',
}

exec { 'apt-get-update':
    command     => '/usr/bin/apt-get update',
    refreshonly => true,
}

package { 'curl':
    ensure => latest,
    require => Exec['apt-get-update'],
}

exec { "debian-add-release":
    command => "curl --silent http://packages.madhouse-project.org/debian/add-release.sh | sh",
    path    => "$PATH",
    require => Package['curl'],
}

$packages = [
  'pkg-config',
  'flex',
  'bison',
  'xsltproc',
  'docbook-xsl',
  'libevtlog-dev',
  'libnet1-dev',
  'libglib2.0-dev',
  'libdbi0-dev',
  'libssl-dev',
  'libjson0-dev',
  'libwrap0-dev',
  'libpcre3-dev',
  'libcap-dev',
  'libesmtp-dev',
  'libgeoip-dev',
  'libhiredis-dev',
  'sqlite3',
  'libdbd-sqlite3',
  'libriemann-client-dev',
  'git',
  'glib2.0',
  'vim',
  'autoconf',
  'automake',
  'libtool',
  'gettext',
  'zsh',
]

package { $packages:
    ensure => latest,
    require    => [ Exec['apt-get-update'], Exec['debian-add-release'] ],
}

class { 'python':
  version    => 'system',
  pip        => true,
  dev        => true,
  virtualenv => true,
  gunicorn   =>  false,
}

python::virtualenv { 'syslog-ng-virtualenv' :
    ensure       => present,
    version      => 'system',
    requirements => "$PROJECT_HOME/requirements.txt",
    systempkgs   => true,
    distribute   => false,
    venv_dir     => "$VAGRANT_HOME/.syslog-ng-virtualenv",
    owner        => 'vagrant',
    group        => 'users',
    cwd          => "$VAGRANT_HOME/.syslog-ng-virtualenv",
    timeout      =>  1800,
}

file { "$VAGRANT_HOME/project":
   ensure => 'link',
   target =>  "$PROJECT_HOME",
}

$shell_profile_files = [
  "$VAGRANT_HOME/.bash_profile",
  "$VAGRANT_HOME/.zprofile"
]

file { $shell_profile_files :
  ensure  => present,
  owner   => 'vagrant',
  group   => 'users',
  mode    => '0664',
  content => 'source .syslog-ng-virtualenv/bin/activate',
}

exec { "autogen":
  command => "bash autogen.sh",
  path    => "$PATH",
  cwd     => "$PROJECT_HOME",
  user    => 'vagrant',
  group   => 'users',
  require => Package[$packages],
}

exec { "configure":
  command => "sh configure --with-ivykis=internal --prefix=/home/vagrant/install --enable-pacct --enable-manpages --enable-debug",
  path    => "$PATH",
  cwd     => "$PROJECT_HOME",
  user    => 'vagrant',
  group   => 'users',
  require =>  [ Exec['autogen'], Python::Virtualenv['syslog-ng-virtualenv'], ],
}

exec { "make":
  command => "make",
  path    => "$PATH",
  cwd     => "$PROJECT_HOME",
  user    => 'vagrant',
  group   => 'users',
  require =>  Exec['configure'],
}

exec { "install":
  command => "make install",
  path    => "$PATH",
  cwd     => "$PROJECT_HOME",
  user    => 'vagrant',
  group   => 'users',
  require =>  Exec['make'],
}

