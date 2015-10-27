#!/usr/bin/env python
#
# Copyright 2015 Michael Rodler <contact@f0rki.at>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

from __future__ import print_function
import sys
import os.path
import argparse
import logging

logging.basicConfig()
log = logging.getLogger(__name__)


DESC = "Parse C header file and generate LLTap tracing hooks."


def construct_argparser():
    parser = argparse.ArgumentParser(description=DESC)
    parser.add_argument("--output", "-o",
                        type=argparse.FileType('w'),
                        help="path to output .c file")
    parser.add_argument("--module", "-m",
                        help="Give a module name to the generated tracer.")
    parser.add_argument("--parser",
                        choices=["libclang"],
                        default="libclang",
                        help="parsing backend to use to parse header files")
    parser.add_argument("--from-lists",
                        action='store_true',
                        help="instead of parsing ")
    # TODO: implement possibility to specify where to search for header files
    #parser.add_argument("--include-dirs",
    #                    nargs="+",
    #                    help="")
    parser.add_argument("-vv",
                        action='store_true',
                        help="turn on very verbose logging")
    parser.add_argument("headers",
                        nargs="+",
                        help="Header files to parse")
    return parser


def do_it(argv):
    ap = construct_argparser()
    args = ap.parse_args(argv)
    if args.parser == "libclang":
        from tracergen.withclang import generate_hooks_from_headers
    else:
        raise NotImplemented("such a parsing backend is not available")
    if not args.headers or len(args.headers) == 0:
        print("No header files given", file=sys.stderr)
        ap.print_help()
        sys.exit(1)
    if args.vv:
        log.setLevel(logging.DEBUG)
    module = args.module
    if not args.module and args.output:
        p = os.path.basename(args.output.name)
        module = p
    headers = None
    include_dir = "/usr/include/"
    if args.from_lists:
        headers = []
        for hlf in args.headers:
            with open(hlf) as f:
                for line in f.readlines():
                    line = line.strip()
                    if not line.startswith("#"):
                        line = os.path.join(include_dir, line)
                        headers.append(line)
    else:
        headers = args.headers
    s = generate_hooks_from_headers(headers, module)
    if args.output:
        args.output.write(s)
    else:
        print(s)
