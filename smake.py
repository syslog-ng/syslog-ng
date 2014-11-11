#!/usr/bin/python

import sys
sys.path.append("../lib")
import os
import Utils
import argparse

from smake import builder

def get_main_options():
    usage = 'usage: bootstrap.py [options] [command [command-args]]'
    epilog = "Build a submodule"
    parser = argparse.ArgumentParser(add_help=False, epilog=epilog, usage=usage)
    parser.add_argument("-h", action="store_true", dest="help", help="Show this help message and exit")
    parser.add_argument("--help", action='store_true', dest='help', help='Show manual page');
    parser.add_argument('command', nargs=argparse.REMAINDER, help='Optional command and it\'s options', default='full')
    return parser


def handle_command(obj, command):
    parameter = None
    if len(command) == 0:
        cmd = 'full'
    else:
        cmd = command[0]
        parameter = " ".join(command[1:])

    try:
        func = getattr(obj, cmd)
        if parameter:
            func(parameter)
        else:
            func()
    except AttributeError:
        print "Unknown command: %s"%cmd

def main(args):
    options = get_main_options()
    opts = options.parse_args(args[1:])
    obj = builder.get_builder()
    if (len(opts.command) and opts.command[0] == 'help' or
        len(opts.command) <= 1 and opts.help):
        options.print_help()
        obj.print_commands()
        return
    
    handle_command(obj, opts.command)

if __name__ == "__main__":
    main(sys.argv)
