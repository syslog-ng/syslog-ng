if ENABLE_EBPF

module_LTLIBRARIES      += modules/ebpf/libebpf.la
modules_ebpf_libebpf_la_SOURCES = \
  modules/ebpf/ebpf-grammar.y       \
  modules/ebpf/ebpf-parser.c        \
  modules/ebpf/ebpf-parser.h        \
  modules/ebpf/ebpf-plugin.c        \
  modules/ebpf/ebpf-reuseport.c        \
  modules/ebpf/ebpf-reuseport.h

modules_ebpf_libebpf_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/modules/ebpf -I$(top_builddir)/modules/ebpf
modules_ebpf_libebpf_la_LIBADD = $(MODULE_DEPS_LIBS) $(LIBBPF_LIBS)
modules_ebpf_libebpf_la_LDFLAGS = $(MODULE_LDFLAGS)
EXTRA_modules_ebpf_libebpf_la_DEPENDENCIES = $(MODULE_DEPS_LIBS)

modules/ebpf modules/ebpf/ mod-ebpf: modules/ebpf/libebpf.la

BPF_CFLAGS=-nostdinc -isystem `$(BPF_CC) -print-file-name=include` \
	-target bpf \
	-Imodules/ebpf -I$(LIBBPF_INCLUDE) \
	-fPIC -O2 -g

%.kern.o: %.kern.c modules/ebpf/vmlinux.h
	$(BPF_CC) $(BPF_CFLAGS) -c $< -o $@

%.skel.c: %.kern.o
	$(BPFTOOL) gen skeleton $< > $@

modules/ebpf/vmlinux.h:
	mkdir -p $(dir $@)
	$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c >$@

CLEANFILES += modules/ebpf/random.skel.c modules/ebpf/vmlinux.h

BUILT_SOURCES += modules/ebpf/random.skel.c


endif

BUILT_SOURCES       +=      \
  modules/ebpf/ebpf-grammar.y       \
  modules/ebpf/ebpf-grammar.c       \
  modules/ebpf/ebpf-grammar.h

EXTRA_DIST        +=      \
  modules/ebpf/ebpf-grammar.ym \
  modules/ebpf/CMakeLists.txt	\
  modules/ebpf/random.kern.c



.PHONY: modules/ebpf/ mod-ebpf
