# Contributor: Michael Pirogov <vbnet.ru@gmail.com>
# Contributor: jv <jens@eisfair.org>
# Contributor: Adrian Guenter <adrian@gntr.me>
# Contributor: Jakub Jirutka <jakub@jirutka.cz>
# Contributor: jv <jens@eisfair.org>
# Maintainer: László Várady <laszlo.varady@axoflow.com>
pkgname=syslog-ng
pkgver=4.0.1
pkgrel=0
pkgdesc="Next generation logging daemon"
url="https://www.syslog-ng.com/products/open-source-log-management/"
arch="all"
license="GPL-2.0-or-later"
options="!check" # unit tests require https://github.com/Snaipe/Criterion with deps
makedepends="
	bison
	curl-dev
	file
	flex
	geoip-dev
	glib-dev
	hiredis-dev
	ivykis-dev>=0.42.4
	json-c-dev
	libdbi-dev
	libmaxminddb-dev
	libnet-dev
	librdkafka-dev
	libtool
	libxml2-utils
	mongo-c-driver-dev
	net-snmp-dev
	openssl-dev>3
	paho-mqtt-c-dev
	pcre-dev
	python3-dev
	rabbitmq-c-dev
	riemann-c-client-dev
	tzdata
	"
subpackages="
	$pkgname-scl::noarch
	$pkgname-dev
	$pkgname-doc
	py3-$pkgname:_python3
	"
source="https://github.com/balabit/syslog-ng/releases/download/syslog-ng-$pkgver/syslog-ng-$pkgver.tar.gz"
builddir="$srcdir/$pkgname-$pkgver"

_modules="
	add-contextual-data
	amqp:afamqp
	examples
	geoip2:geoip2-plugin
	graphite
	http
	json:json-plugin
	kafka
	map-value-pairs
	mongodb:afmongodb
	mqtt
	redis
	riemann
	snmp:afsnmp
	sql:afsql
	stardate
	stomp:afstomp
	tags-parser
	xml
	"
for _i in $_modules; do
	subpackages="$subpackages $pkgname-${_i%%:*}:_module"
done

build() {
	CFLAGS="$CFLAGS -flto=auto" \
	./configure \
		--prefix=/usr \
		--sysconfdir=/etc/syslog-ng \
		--localstatedir=/run \
		--enable-extra-warnings \
		--enable-ipv6 \
		--enable-manpages \
		--with-ivykis=system \
		--with-jsonc=system \
		\
		--enable-all-modules \
		--disable-linux-caps \
		--disable-smtp \
		--disable-systemd \
		--disable-java \
		--disable-java-modules
	make
}

package() {
	make DESTDIR="$pkgdir" install

	cd "$pkgdir"

	rm -rf run usr/lib/$pkgname/libtest

	# getent module doesn't build properly as musl doesn't support reentrant
	# getprotoby[number|name] funcs. The provided compat lib only patches
	# solaris, which does provide reentrant versions under a different sig
	rm -f usr/lib/$pkgname/libtfgetent.so

	# Remove static file
	rm -f usr/lib/libsyslog-ng-native-connector.a

	install -d -m 755 etc/$pkgname/conf.d
	install -d -m 700 "$pkgdir"/var/lib/syslog-ng

	rm "$pkgdir"/usr/lib/syslog-ng/python/requirements.txt
}

scl() {
	pkgdesc="$pkgdesc (configuration library)"
	depends="$pkgname=$pkgver-r$pkgrel"

	_submv usr/share/syslog-ng/include/scl
}

dev() {
	default_dev

	_submv usr/share/syslog-ng/tools \
		usr/share/syslog-ng/xsd
}

_python3() {
	pkgdesc="$pkgdesc (python3 module)"

	_submv usr/lib/syslog-ng/libmod-python.so

	local site_pkgs="$(python3 -c 'import site; print(site.getsitepackages()[0])')"

	mkdir -p "$subpkgdir"/"$site_pkgs"
	mv "$pkgdir"/usr/lib/syslog-ng/python/* \
		"$subpkgdir"/"$site_pkgs"
}

_module() {
	local name="${subpkgname#$pkgname-}"
	pkgdesc="$pkgdesc (${name//-/ } module)"

	local libname=$(printf '%s\n' $_modules | grep "^$name:" | cut -d: -f2)
	local soname="lib${libname:-$name}.so"

	_submv usr/lib/syslog-ng/$soname
}

_submv() {
	local path; for path in "$@"; do
		mkdir -p "$subpkgdir/${path%/*}"
		mv "$pkgdir"/$path "$subpkgdir"/${path%/*}/
	done
}

sha512sums="5f83ee3cc4935218feb19f3f5065a68099e3ee291d806ad8810499ded9f9ef3b326b4b22841cd736354ed6a2ebc1ce8ae73f6abe981aa6f64c42da9ee3b1e22f  syslog-ng-4.0.1.tar.gz"
