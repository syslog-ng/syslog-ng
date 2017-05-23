EXTRA_DIST = rules rules.conf pkginfo.dev pkginfo.lib pkgmaker.sh prototype-maker.sh admin pkginfo.dev.in pkginfo.lib.in

pkginfo.dev: pkginfo.dev.in
	$(top_builddir)/config.status --file $@:$< 

pkginfo.lib: pkginfo.lib.in
	$(top_builddir)/config.status --file $@:$< 
