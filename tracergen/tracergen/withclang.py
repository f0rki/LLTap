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
"""
Uses libclang as a C/C++ header parser.
"""

from __future__ import print_function
import sys
import os
import datetime

try:
    import clang.cindex  # NOQA
    from clang.cindex import CursorKind, TypeKind  # NOQA
except ImportError as e:
    llsrc = os.getenv("PY_LIBCLANG")
    if not llsrc:
        print("Set a PY_LIBCLANG environment var containing the path to the"
              "python libclang bindings inside the clang/llvm source tree.",
              "Make sure that PATH is configured such that llvm-config is",
              "the same version as the libclang python bindings",
              file=sys.stderr, sep="\n")
    sys.path.append(llsrc)
    import clang.cindex  # NOQA
    from clang.cindex import CursorKind, TypeKind  # NOQA
    import subprocess
    libdir = subprocess.check_output("llvm-config --libdir")
    LIBCLANG = os.path.join(libdir, "libclang.so")
    clang.cindex.Config.set_library_file(LIBCLANG)

from tracergen.templates import gen_hook_function_code, gen_hook_file  # NOQA


CFLAGS = ['-x', 'c', '-std', 'gnu99']


def filep_accessor(argname):
    return "fileno=%d", "fileno(*{})".format(argname), "", ["stdio.h"]


class Accessor:
    """
    Small class that contains necessary information on how to access and
    argument and the corresponding printf style format string.
    """
    __slots__ = ["code", "includes", "accessor", "fmt"]

    def __init__(self, code=None, includes=None, accessor=None, fmt=None):
        self.code = code
        self.includes = includes
        self.accessor = accessor
        self.fmt = fmt

    def __repr__(self):
        return "Accessor({!r}, {!r}, {!r}, {!r})".format(self.code,
                                                         self.includes,
                                                         self.accessor,
                                                         self.fmt)

    def __str__(self):
        return repr(self)


class ArgType:
    """
    Class that wraps a Type and offers an interface to get a format string and
    code to access the relevant data.
    """

    TYPE_FMT_MAP = {
        "char": "%c",
        "unsigned char": "%hhu",
        "short": "%hd",
        "unsigned short": "%hu",
        "int": "%d",
        "unsigned int": "%u",
        "long": "%ld",
        "unsigned long": "%lu",
        "long long": "%lld",
        "unsigned long long": "%llu",
        "float": "%f",
        "double": "%f",
        "long double": "%Lf",
        "void*": "%p",
        "char*": "\\\"%s\\\"",
        "size_t": "%zd",
    }
    SPECIAL_ACCESSOR = {
        "FILE*": filep_accessor,
    }
    TYPE_FMT_DEFAULT = "%p"

    @staticmethod
    def get_default_accessor(argname):
        return "(void*)" + argname

    def __init__(self, arg):
        if isinstance(arg, clang.cindex.Type):
            self.argtype = arg
            self._arg = None
        else:
            self._arg = arg
            self.argtype = arg.type

        sp = self.argtype.spelling
        self.argtype_spelling = self.__get_usable_type_spelling(sp)
        sp = self.argtype.get_canonical().spelling
        self.argtype_canonical_spelling = self.__get_usable_type_spelling(sp)
        if self.argtype.kind == TypeKind.POINTER:
            sp = self.argtype.get_pointee().spelling
            self.pointee_type_spelling = self.__get_usable_type_spelling(sp)
        else:
            self.pointee_type_spelling = None

    def __repr__(self):
        s = "ArgType('{}')  # kind={}, canonical={}, spellings={},{}"\
            .format(self.argtype.spelling,
                    self.argtype.kind.spelling,
                    self.argtype.get_canonical().spelling,
                    self.argtype_spelling,
                    self.argtype_canonical_spelling)
        return s

    def __get_usable_type_spelling(self, spelling):
        """remove some annoying modifyers from the type spelling.
        oh god this is such a terrible hack..."""
        s = spelling.replace("restrict", "")
        s = s.replace("const", "")
        s = s.replace("volatile", "")
        s = s.replace(" *", "*")
        s = s.strip()
        return s

    def is_pointer_type(self):
        return self.argtype.kind == TypeKind.POINTER

    @property
    def fmt(self):
        if self.argtype_spelling in ArgType.TYPE_FMT_MAP:
            return ArgType.TYPE_FMT_MAP[self.argtype_spelling]
        if self.argtype_canonical_spelling in ArgType.TYPE_FMT_MAP:
            return ArgType.TYPE_FMT_MAP[self.argtype_canonical_spelling]
        if self.is_pointer_type():
            return "(" + self.argtype.spelling + ") %p"
        return None

    def accessor_for(self, argname):
        """
        Create accessor function according to type information of this object
        and for the given argname.
        """
        acc = Accessor()
        if self.argtype_spelling in ArgType.TYPE_FMT_MAP:
            # primitive type which has a format string
            acc.fmt = self.fmt
            acc.accessor = "*" + argname
        elif self.argtype_spelling in ArgType.SPECIAL_ACCESSOR:
            a = ArgType.SPECIAL_ACCESSOR[self.argtype_spelling]
            acc.fmt, acc.accessor, acc.code, acc.includes = a(argname)
        elif self.argtype_canonical_spelling in ArgType.SPECIAL_ACCESSOR:
            a = ArgType.SPECIAL_ACCESSOR[self.argtype_canonical_spelling]
            acc.fmt, acc.accessor, acc.code, acc.includes = a(argname)
        elif self.is_pointer_type() \
                and self.pointee_type_spelling in ArgType.TYPE_FMT_MAP:
            acc.fmt = ArgType.TYPE_FMT_MAP[self.pointee_type_spelling]
            acc.accessor = "**" + argname
            acc.code, acc.includes = None, None
        elif self.fmt:
            acc.fmt = self.fmt
            if "%p" in acc.fmt:
                acc.accessor = "(void*) *" + argname
            else:
                acc.accessor = "*" + argname
        else:
            acc.fmt = ArgType.TYPE_FMT_DEFAULT
            acc.accessor = ArgType.get_default_accessor(argname)
        return acc


class RetType(ArgType):
    pass


class HookFunction:
    """
    Class that represents a generated hook function.
    """

    def __init__(self, node, hooktype):
        self.node = node
        self.origfunc = node
        self.type = hooktype
        self.code = None
        self.globalvars = []
        self.includes = []
        self.name = "_gen_" + self.origfunc.spelling + "_hook_" + hooktype
        self.target = self.origfunc.spelling

    def __str__(self):
        return """
{includes}

{globalvars}

{code}
""".format(includes=self.get_includes(),
           globalvars=self.get_globalvars(),
           code=self.code)

    def generate_code(self, custom_templates={}):
        self.code = self.get_hook_code(custom_templates)

    def get_hook_proto_args(self):
        """
        Formats the hook function arguments as the list of arguments for the C
        function prototype.
        """
        args = []
        if self.type == "post":
            args.append("{}* ret".format(self.node.result_type.spelling))
        for i, arg in enumerate(self.node.get_arguments()):
            if self.type != "replace":
                argt = ArgType(arg.type)
                args.append("{}* arg{}".format(argt.argtype_spelling, i))
            else:
                args.append("{} arg{}".format(arg.type.spelling, i))
        if self.is_variadic():
            args.append("...")
        return ", ".join(args)

    def get_hook_code(self, custom_templates={}):
        """
        Generate the code of the hook and return it as a string.
        """
        self.includes = []
        d = {}
        d["hook"] = self
        d["return_type"] = "void"
        d["hook_args"] = self.get_hook_proto_args()
        prepcode = []
        if self.type == "pre":
            d.update({"function": self.node.spelling, "prepcode": ""})
            fmtstr = []
            args = []
            for i, arg in enumerate(self.node.get_arguments()):
                argt = ArgType(arg)
                a = argt.accessor_for("arg{}".format(i))
                fmtstr.append("{}".format(a.fmt))
                args.append(a.accessor)
                if a.code:
                    prepcode.append(a.code)
                if a.includes:
                    self.includes.extend(a.includes)
            if self.is_variadic():
                fmtstr.append("...")
            d["fmtstr"] = ", ".join(fmtstr)
            if args:
                d["args"] = ", " + ", ".join(args)
            else:
                d["args"] = ""
        elif self.type == "post":
            d.update({"rettype": self.node.result_type.spelling})
            if self.node.result_type.spelling != "void":
                rett = RetType(self.node.result_type)
                a = rett.accessor_for("ret")
                d["retfmt"] = "{}".format(a.fmt)
                if a.code:
                    prepcode.append(a.code)
                if a.includes:
                    self.includes.extend(a.includes)
                d["retval"] = ", " + a.accessor
            else:
                d["retfmt"] = ""
                d['retval'] = ""
        if prepcode:
            d["prepcode"] = "\n".join(prepcode)
        else:
            d["prepcode"] = None
        return gen_hook_function_code(d, custom_templates)

    def get_globalvars(self):
        if self.globalvars:
            return "\n".join(self.globalvars)
        else:
            return ""

    def get_includes(self):
        return self.includes

    def get_code(self):
        return self.code

    def is_variadic(self):
        if self.node.type.kind == TypeKind.FUNCTIONNOPROTO:
            return False
        return self.node.type.is_function_variadic()

    @property
    def type_in_C(self):
        if self.type == "pre":
            return "LLTAP_PRE_HOOK"
        elif self.type == "post":
            return "LLTAP_POST_HOOK"
        elif self.type == "replace":
            return "LLTAP_REPLACE_HOOK"


def generate_hooks(node, tu):
    """for a given declaration (node) create pre and post hook functions"""
    print("Generating hooks for function",
          node.result_type.spelling, node.displayname)
    d = []
    for t in ("pre", "post"):
        hook = HookFunction(node, t)
        hook.generate_code()
        d.append(hook)
    return d


def find_decls(node):
    """returns a list of all function declaration ast nodes"""
    decls = []
    if node.kind.is_declaration() and node.kind == CursorKind.FUNCTION_DECL:
        #assert node.type.kind == TypeKind.FUNCTIONPROTO, \
        #        "not a function prototype but a " + node.type.kind.spelling
        decls.append(node)
    # Recurse for children of this node
    for c in node.get_children():
        decls.extend(find_decls(c))
    return decls


def main(argv):
    if len(argv) != 3:
        print("Invalid number of arguments")
        print("Usage:", argv[0], "path/to/new/module", "path/to/header.h")
        return
    index = clang.cindex.Index.create()
    print("parsing ", argv[2])
    tu = index.parse(argv[2])
    if not tu:
        print("failed to parse", argv[2])
    module = os.path.basename(argv[2])
    with open(argv[1] + ".c", "w") as f:
        includes = ["liblltap.h"]
        globalvars = []
        hooks = []
        hooknames = {"pre": {}, "post": {}, "replace": {}}
        for node in find_decls(tu.cursor):
            for h in generate_hooks(node, tu):
                if h.target not in hooknames[h.type]:
                    hooknames[h.type][h.target] = h.name
                    includes.extend(h.get_includes())
                    globalvars.append(h.get_globalvars())
                    hooks.append(h)
                else:
                    print("warning: skipping second hook for", h.target)
        now = datetime.datetime.now().isoformat()
        s = gen_hook_file(dict(header_files=argv[2],
                               date=now,
                               includes=set(includes),
                               global_variables=globalvars,
                               hooks=hooks,
                               module=module))
        print(s)
        f.write(str(s))

if __name__ == "__main__":
    main(sys.argv)
