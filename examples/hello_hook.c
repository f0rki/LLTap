#include <liblltap.h>
#include <stdio.h>

void hello_hook(char** name) {
  fprintf(stderr, "say_hello(\"%s\") - Changing arg to \"dlroW\"\n", *name);
  *name = "dlroW";
}

LLTAP_REGISTER_HOOK("say_hello", hello_hook, LLTAP_PRE_HOOK)
