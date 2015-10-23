#include <liblltap.h>

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

int countdown = 3;
bool passthrough = false;

void* malloc_replace_hook(size_t size) {
  if (! passthrough && countdown == 0) {
    passthrough = true;
    return NULL;
  } else {
    countdown--;
    return malloc(size);
  }
}

void malloc_post_hook(void** ptr, size_t size)
{
  fprintf(stderr, "[ALLOCATION] allocated %p of size %zu\n", *ptr, size);
}

void free_pre_hook(void* ptr)
{
  fprintf(stderr, "[ALLOCATION] freeing: %p\n", ptr);
}


/*LLTAP_REGISTER_HOOK("puts", puts_hook, LLTAP_REPLACE_HOOK)*/
LLTAP_HOOKSV my_hooks[] = {
    {"malloc", (LLTapHook) malloc_post_hook, LLTAP_POST_HOOK},
    {"malloc", (LLTapHook) malloc_replace_hook, LLTAP_REPLACE_HOOK},
    {"free", (LLTapHook) free_pre_hook, LLTAP_PRE_HOOK},
    LLTAP_HOOKSV_END,
    };
LLTAP_REGISTER_HOOKS(my_hooks)
