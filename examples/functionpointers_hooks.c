#include <liblltap.h>

#include <stdio.h>


void lower_hook_pre(char** s)
{
  printf("to_lower called on '%s'\n", *s);
}


void upper_hook_pre(char** s)
{
  printf("to_upper called on '%s'\n", *s);
}


LLTAP_HOOKSV my_hooks[] = {
  {"to_lower", (LLTapHook) lower_hook_pre, LLTAP_PRE_HOOK},
  {"to_upper", (LLTapHook) upper_hook_pre, LLTAP_PRE_HOOK},
  LLTAP_HOOKSV_END,
};
LLTAP_REGISTER_HOOKS(my_hooks)
