/*
 * Copyright 2015 Michael Rodler <contact@f0rki.at>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef LIBLLTAP_H
#define LIBLLTAP_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

/**** Data structures ****/

typedef void* LLTapHook;

enum LLTapHookType {
  LLTAP_PRE_HOOK = 1,
  LLTAP_REPLACE_HOOK = 2,
  LLTAP_POST_HOOK = 4,
};
#ifndef __cplusplus
typedef enum LLTapHookType LLTapHookType;
#endif

struct LLTapHookInfo {
  char* target;
  LLTapHook hook;
  LLTapHookType type;
};
#ifndef __cplusplus
typedef struct LLTapHookInfo LLTapHookInfo;
#endif

int lltap_register_hook(char* target, LLTapHook hook, LLTapHookType type);
void lltap_deregister_hook(char* target, LLTapHookType type);
int lltap_register_hook_i(LLTapHookInfo* reg);

#define LLTAP_REGISTER_HOOK(target, hookfunction, hooktype) \
void __attribute__((constructor)) __LLTapHook_init(void) { \
  lltap_register_hook(target, (LLTapHook) &hookfunction, hooktype);\
}

#define LLTAP_HOOKSV LLTapHookInfo
#define LLTAP_HOOKSV_END {NULL, NULL, 0}
#define LLTAP_REGISTER_HOOKS(__lltap_hooks) \
void __attribute__((constructor)) __LLTapHook_init(void) { \
  for (size_t i = 0; __lltap_hooks[i].target != NULL; ++i) { \
    lltap_register_hook(__lltap_hooks[i].target, \
        __lltap_hooks[i].hook, __lltap_hooks[i].type); \
  } \
} \


void __lltap_inst_add_hook_target(void* addr, char* name);
LLTapHook __lltap_inst_get_hook(void* target, LLTapHookType type);
int __lltap_inst_has_hooks(void* target);

#ifdef __cplusplus
}
#endif
#endif // LIBLLTAP_H
