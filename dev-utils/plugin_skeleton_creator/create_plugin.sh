#!/bin/bash

top_srcdir="$(git rev-parse --show-toplevel)"

function print_help {
  echo "This script help to create new plugin with creating skeleton"
  echo "Parameters:"
  echo -e "\t-n\tName of the plugin (required)"
  echo -e "\t-k\tThe keyword in config file (default: name of the plugin)"
  echo -e "\t-t\tType of the plugin (default: LL_CONTEXT_DESTINATION)"
  echo -e "\t-d\tPlugin root dir (default: ${top_srcdir}/modules)"
  echo -e "\t-h\tPrint this help message"
}

plugin_name=''
plugin_key=''
plugin_type=LL_CONTEXT_DESTINATION
plugin_root="${top_srcdir}/modules"
template_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

help=0
file_list="plugin_template_grammar.ym
           plugin_template_Makefile.am
           plugin_template_parser.c
           plugin_template_parser.h
           plugin_template_plugin.c"

while getopts "k:n:t:h" opt; do
  case ${opt} in
    h)
      help=1
      ;;
    n)
      plugin_name="${OPTARG}"
      ;;
    t)
      plugin_type="${OPTARG}"
      ;;
    k)
      plugin_key="${OPTARG}"
      ;;
    d)
      plugin_root="${OPTARG}"
      ;;
  esac
done

if test "${help}" -gt 0; then
  print_help
  exit 0
fi

if [ -z "${plugin_name}" ]; then
  echo "Please set the plugin name"
  print_help
  exit 1
fi

plugin_dir=${plugin_root}/${plugin_name}

if ! [ -d "${plugin_root}" ]; then
  echo "Plugin root directory does not exists: ${plugin_root}"
  echo "Please set with -d parameter"
  print_help
  exit 1
fi

if [ -d "${plugin_dir}" ]; then
  echo "Plugin already exists: ${plugin_dir}"
  exit 1
fi 

if [ -z "${plugin_key}" ]; then
  plugin_key=${plugin_name}
fi

echo "plugin name = ${plugin_name}"
echo "plugin type = ${plugin_type}"
echo "plugin key = ${plugin_key}"


mkdir ${plugin_dir}

for filename in ${file_list}; do
  plugin_name_under_score=`echo ${plugin_name} | sed "s/-/_/g"`
  if [ "${filename}" == "plugin_template_Makefile.am" ]; then
    dst_filename="Makefile.am"
  else
    dst_filename=`echo ${filename} | sed "s/plugin_template_/${plugin_name}-/g"`
  fi
  dst_filename=${plugin_dir}/${dst_filename}
  cp ${template_dir}/${filename} ${dst_filename}
  sed -i "s/@PLUGIN_NAME@/${plugin_name}/g" ${dst_filename}
  sed -i "s/@PLUGIN_NAME_US@/${plugin_name_under_score}/g" ${dst_filename}
  sed -i "s/@PLUGIN_TYPE@/${plugin_type}/g" ${dst_filename}
  sed -i "s/@PLUGIN_KEY@/${plugin_key}/g" ${dst_filename}
done
echo "Done: new plugin skeleton is created in ${plugin_dir}"
