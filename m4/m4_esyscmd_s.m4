
# This implementation was copied from
#    https://github.com/Netflix/dynomite/commit/8c0b28d131f581835c7d0fcf195836bbd4aed4bf
#
# Although there's no explicit license text on the configure.ac file where
# this was added, dynomite is under the Apache 2.0 license, so this is
# covered by that as well.
#
# Author in the Dynomite codebase:
#   Akbar Ahmed, https://github.com/akbarahmed
#
# Merged into Dynomite by:
#   Shailesh Birari, https://github.com/shailesh33

# Add support for m4_esyscmd_s to autoconf-2.63 (used in RHEL/CentOS 6.7)
m4_ifndef([m4_esyscmd_s], [m4_define([m4_chomp_all], [m4_format([[%.*s]], m4_bregexp(m4_translit([[$1]], [/], [/ ]), [/*$]), [$1])])])
m4_ifndef([m4_esyscmd_s], [m4_define([m4_esyscmd_s], [m4_chomp_all(m4_esyscmd([$1]))])])
