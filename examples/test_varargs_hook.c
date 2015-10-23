#include <stdio.h>
#include <stdarg.h>
#include "../lib/include/liblltap.h"


void fu_hook_pre(int* argc, ...)
{
  va_list ap;
  fputs("fu_hook_pre: will print ", stdout);
  va_start(ap, argc);
  for (int i = 0; i < *argc; ++i) {
    char** sp = va_arg(ap, char**);
    fprintf(stdout, "'%s', ", *sp);
  }
  puts("");
  va_end(ap);
}


void fu_hook_post(int* ret, int argc, ...)
{
  va_list ap;
  va_start(ap, argc);
  fputs("fu_hook_post: has printed ", stdout);
  for (int i = 0; i < argc; ++i) {
    char* sp = va_arg(ap, char*);
    fprintf(stdout, "'%s', ", sp);
  }
  printf(" and returned '%d'\n", *ret);
  puts("forcing return of 0");
  *ret = 0;
  va_end(ap);
}


int fu_hook_replace(int argc, ...)
{
  (void)argc;
  puts("fu_hook_replace: called instead of original fu");
  return -1;
}

LLTAP_HOOKSV my_hooks[] = {
    {"fu", (LLTapHook) fu_hook_pre, LLTAP_PRE_HOOK},
    {"fu", (LLTapHook) fu_hook_post, LLTAP_POST_HOOK},
    {"fu", (LLTapHook) fu_hook_replace, LLTAP_REPLACE_HOOK},
    LLTAP_HOOKSV_END,
    };
LLTAP_REGISTER_HOOKS(my_hooks)
