include(LibFindMacros)

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(Riemann_PKGCONF riemann-client)

# Finally the library itself
find_library(Riemann_LIBRARY
  NAMES riemann-client
  PATHS ${Riemann_PKGCONF_LIBRARY_DIRS}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(Riemann_PROCESS_LIBS Riemann_LIBRARY)

# silence warnings
set(Riemann_FIND_QUIETLY 1)
libfind_process(Riemann)
