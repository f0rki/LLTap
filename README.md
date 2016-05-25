# LLTap - Compile Time Function Call Instrumentation

LLTap is a tool to easily add instrumentation code around function calls. As a
user one can specify pre and post hooks, which are called before and after the
target function, or replace the function call completely.

Possible uses would be:

  * as a debugging utility: print debug information and possibly modify
      arguments or return values.
  * as a call tracer: trace all calls to a certain API, similar to ltrace but
      for an arbitrary set of functions.
  * as a testing tool: perform fault injection - let some API calls fail or
      modify the result values.

Instrumentation works on LLVM Bitcode, so instrumenting binaries is not
possible unless a native code to bitcode decompiler is used.


## Building

First you need to have LLVM and clang and their development headers installed.
LLTap uses cmake to generate the build files. To build LLTap using ninja:

    mkdir build
    cd build
    cmake -G Ninja ..
    ninja

Which will compile the LLVM pass and runtime support library.

## Usage

Consider the following snippet of C code:

```
#include <stdio.h>

void say_hello(char* name)
{
  printf("Hello %s!\n", name);
}

int main()
{
  say_hello("World");
}
```

Let's say we want to modify the call to say_hello. We can create the following
hook, which prints the argument to `say_hello` and modifies it.

```
#include <liblltap.h>
#include <stdio.h>

void hello_hook(char** name) {
  fprintf(stderr, "say_hello(\"%s\") - Changing arg to \"dlrow\"\n", *name);
  *name = "dlroW";
}

LLTAP_REGISTER_HOOK("say_hello", hello_hook, LLTAP_PRE_HOOK)
```

Note that a pre hook can modify the parameters of the original call, which are
passed as pointers to the pre hook.

Now to build with instrumentation, we need to compile to LLVM bitcode. Apply
the LLTap Instrumentation pass to the bitcode file.
```
clang -emit-llvm -c hello.c
opt -load ../build/llvmpass/libLLTap.so -LLTapInst hello.bc -o hello_inst.bc
clang -I ../include -L ../build/lib/ hello_hook.c hello_inst.bc -o hello -llltaprt
```
Note that it is also possible to streamline this build process by loading the
LLTap pass directly from `clang` using the `-Xclang` argument. Care has to be
taken, that hooks are not also instrumented when building, so that infinite
call loops are avoided.


If we run the resulting binary:

```
$ env LD_LIBRARY_PATH=../build/lib/ ./hello
say_hello("World") - Changing arg to "dlroW"
Hello dlroW!
```
We can see that the pre hook has printed some logging code and also changed the
first argument.

## Hook Types

LLTap supports three different hook types: pre, post and replace.

  * `LLTAP_PRE_HOOK` - is executed before the call and receives pointers to the
      parameters of the original call, so that it can modify the parameters,
      e.g.
```
int example(int a, char* b);
void example_prehook(int* a, char** b);
```
  * `LLTAP_REPLACE_HOOK` - is executed instead of the original function and
      must have the same prototype as the function it replaces.
  * `LLTAP_POST_HOOK` - is exectued after the call and receives a pointer to
      the return value as first parameter. Additionally the parameters of the
      original call are passed by value, e.g.
```
void example_posthook(int* ret, int a, char* b);
```

## Automatic Generation of API Tracers

`tracergen/lltaptracergen` is a python script can be used to generate tracing
hooks given one ore more C header files. This can be used to create a tool
similar to ltrace. To generate tracing hooks for stdio.h, execute:

    ./lltaptracergen -o stdio.c -m stdio /usr/include/stdio.h

This produces a C file, that contains LLTap hooks, which print the function
name, arguments and the return value.

You will need the python libclang bindings for this tool to work. At the time
of writing they are only included in the clang source distribution, so you
might need to set some environment variables so that python can import it:

    # to find llvm-config
    export PATH=/path/to/src/llvm/build/bin/:$PATH
    # added to python path
    export PY_LIBCLANG=/path/to/src/llvm/tools/clang/bindings/python


## Related Work

Google has proposed a very similar tool called x-ray at the
[LLVM-dev mailing list](http://lists.llvm.org/pipermail/llvm-dev/2016-April/098901.html).
