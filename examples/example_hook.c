#include <stdio.h>
#include <string.h>
#include <err.h>
#include "liblltap.h"

void example_hook_pre() {
  puts("example_hook - pre");
}

void example_hook_post() {
  puts("example_hook - post");
}


int puts_hook(char* s) {
  printf("Call to 'puts' intercepted.\nfirst arg: '%s'\nderegistering hook\n", s);
  lltap_deregister_hook("puts", LLTAP_REPLACE_HOOK);
  return strlen(s);
}


/*LLTAP_REGISTER_HOOK("puts", puts_hook, LLTAP_REPLACE_HOOK)*/
LLTAP_HOOKSV my_hooks[] = {
    {"example", (LLTapHook) example_hook_pre, LLTAP_PRE_HOOK},
    {"example", (LLTapHook) example_hook_post, LLTAP_POST_HOOK},
    {"puts", (LLTapHook) puts_hook, LLTAP_REPLACE_HOOK},
    LLTAP_HOOKSV_END,
    };
LLTAP_REGISTER_HOOKS(my_hooks)
