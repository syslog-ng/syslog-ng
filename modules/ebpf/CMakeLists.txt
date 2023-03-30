if (NOT DEFINED ENABLE_EBPF OR ENABLE_EBPF)
  find_package(LIBBPF)
endif()

module_switch(ENABLE_EBPF "Enable ebpf module (requires ebpf toolchain)" LIBBPF_FOUND)

if (NOT ENABLE_EBPF)
  return()
endif()

add_custom_command(OUTPUT vmlinux.h
                   COMMAND ${BPFTOOL} btf dump file /sys/kernel/btf/vmlinux format c >vmlinux.h)

add_custom_command(OUTPUT random.skel.c
                   COMMAND ${BPFTOOL} gen skeleton random.kern.o > random.skel.c
		   DEPENDS random.kern.o)

add_custom_command(OUTPUT random.kern.o
                   COMMAND ${BPF_CC} ${BPF_CFLAGS} -c ${CMAKE_CURRENT_SOURCE_DIR}/random.kern.c -o random.kern.o
                   DEPENDS random.kern.c vmlinux.h)

add_custom_target(generate_ebpf_skeletons DEPENDS "random.skel.c")

set(EBPF_SOURCES
    ebpf-parser.h
    ebpf-reuseport.h
    ebpf-reuseport.c
    ebpf-plugin.c
    ebpf-parser.c
)

add_module(
  TARGET ebpf
  GRAMMAR ebpf-grammar
  INCLUDES ${PROJECT_SOURCE_DIR}
  SOURCES ${EBPF_SOURCES}
  DEPENDS ${LIBBPF_LIBRARIES}
)

add_dependencies(ebpf generate_ebpf_skeletons)
