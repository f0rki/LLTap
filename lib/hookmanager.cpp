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

#include <liblltap.h>

#include <map>
#include <list>
#include <cstdio>
#include <mutex>
#include <thread>


using namespace std;

namespace LLTap {

  enum class LogLevel {
    SILENT,
    ERROR,
    WARN,
    DEBUG,
  };

  struct hook_registry {
    LLTapHook pre_hook = nullptr;
    LLTapHook replace_hook = nullptr;
    LLTapHook post_hook = nullptr;
  };

  class HookManager {

    public:
      bool add_hook(char* target, LLTapHook hook, LLTapHookType type);
      void add_target(char* name, void* target);
      LLTapHook get_hook(void* target, LLTapHookType type);
      int get_hook_bitmap(void* target);
      void remove_hook(char* name, LLTapHookType type);

      ~HookManager() {
        if (hooks != nullptr)
          delete hooks;
        if (functions != nullptr)
          delete functions;
      }

      HookManager() {
        check_loglevel();
      }

    private:
      map<void*, hook_registry>* hooks = nullptr;
      map<string, void*>* functions = nullptr;

      mutex hm_mutex;

      LogLevel loglevel = LogLevel::ERROR;

      void check_loglevel() {
        char* x = getenv("LLTAP_LOGLEVEL");
        if (x != nullptr) {
          string r(x);
          if (r == "SILENT") {
            loglevel = LogLevel::SILENT;
          } else if (r == "ERROR") {
            loglevel = LogLevel::ERROR;
          } else if (r == "WARN") {
            loglevel = LogLevel::WARN;
          } else if ( r == "DEBUG") {
            loglevel = LogLevel::DEBUG;
          }
        }
      }
  };

  HookManager hookmanager;
}

/**
 * HookManager implementation
 */

bool LLTap::HookManager::add_hook(char* target, LLTapHook hook, LLTapHookType type) {
  lock_guard<std::mutex> lock(hm_mutex);

  if (loglevel >= LogLevel::DEBUG) {
    fprintf(stderr,
        "[LLTAP-RT] Adding hook for target %s (%p) type %d\n",
        target, (void*)hook, type);
  }

  if (functions == nullptr) {
    if (loglevel >= LogLevel::WARN) {
      fprintf(stderr, "[LLTAP-RT] No hook targets registered\n");
    }
    return false;
  }

  if (hooks == nullptr) {
    hooks = new map<void*, hook_registry>();
  }

  void* targetaddr = (*functions)[target];
  if (target == nullptr) {
    return false;
  }
  switch (type) {
    case LLTAP_PRE_HOOK:
      (*hooks)[targetaddr].pre_hook = hook;
      break;
    case LLTAP_REPLACE_HOOK:
      (*hooks)[targetaddr].replace_hook = hook;
      break;
    case LLTAP_POST_HOOK:
      (*hooks)[targetaddr].post_hook = hook;
      break;
    default:
      if (loglevel >= LogLevel::ERROR) {
        fprintf(stderr, "[LLTAP-RT] Invalid hook type\n");
      }
  }

  return true;
}

LLTapHook LLTap::HookManager::get_hook(void* target, LLTapHookType type) {
  lock_guard<std::mutex> lock(hm_mutex);

  if (hooks == nullptr) {
    if (loglevel >= LogLevel::WARN) {
      fprintf(stderr, "[LLTAP-RT] No hooks registered at all\n");
    }
    return nullptr;
  }

  if (hooks->count(target) != 0) {
    switch (type) {
      case LLTapHookType::LLTAP_PRE_HOOK:
        return (*hooks)[target].pre_hook;
      case LLTapHookType::LLTAP_POST_HOOK:
        return (*hooks)[target].post_hook;
      case LLTapHookType::LLTAP_REPLACE_HOOK:
        return (*hooks)[target].replace_hook;
      default:
        if (loglevel >= LogLevel::ERROR) {
          fprintf(stderr, "[LLTAP-RT] Invalid hook type\n");
        }
        return nullptr;
    }
  } else {
    if (loglevel >= LogLevel::WARN) {
      fprintf(stderr, "[LLTAP-RT] No hooks found for (%p)\n", target);
    }
    return nullptr;
  }
}

int LLTap::HookManager::get_hook_bitmap(void* target) {
  lock_guard<std::mutex> lock(hm_mutex);

  int hook_bm = 0;
  if (hooks == nullptr) {
    if (loglevel >= LogLevel::DEBUG) {
      fprintf(stderr, "[LLTAP-RT] No hooks registered at all\n");
    }
    return 0;
  }

  if (hooks->count(target) != 0) {
    hook_registry& hr = (*hooks)[target];
    if (hr.pre_hook != nullptr) {
      hook_bm |= LLTapHookType::LLTAP_PRE_HOOK;
    }
    if (hr.replace_hook != nullptr) {
      hook_bm |= LLTapHookType::LLTAP_REPLACE_HOOK;
    }
    if (hr.post_hook != nullptr) {
      hook_bm |= LLTapHookType::LLTAP_POST_HOOK;
    }
  }

  return hook_bm;
}

void LLTap::HookManager::remove_hook(char* name, LLTapHookType type) {
  lock_guard<std::mutex> lock(hm_mutex);

  if (hooks == nullptr) {
    return;
  }

  void* target = (*functions)[name];
  if (target == nullptr) {
    return;
  }
  if (hooks->count(target) != 0) {
    switch (type) {
      case LLTapHookType::LLTAP_PRE_HOOK:
        (*hooks)[target].pre_hook = nullptr;
        break;
      case LLTapHookType::LLTAP_POST_HOOK:
        (*hooks)[target].post_hook = nullptr;
        break;
      case LLTapHookType::LLTAP_REPLACE_HOOK:
        (*hooks)[target].replace_hook = nullptr;
        break;
      default:
        if (loglevel >= LogLevel::ERROR) {
          fprintf(stderr,
              "[LLTAP-RT] Failed to remove hook on '%s' - Invalid hook type (%d)\n",
              name, type);
        }
        return;
    }
  }

}

void LLTap::HookManager::add_target(char* name, void* target) {
  lock_guard<std::mutex> lock(hm_mutex);

  if (functions == nullptr) {
    functions = new map<string, void*>();
  }

  if (loglevel >= LogLevel::DEBUG) {
    fprintf(stderr, "[LLTAP-RT] Registering target %s for addr (%p)\n", name, target);
  }
  string n(name);
  (*functions)[n] = target;
}


/**
 * LLTap API wrappers
 */
extern "C" {

int lltap_register_hook(char* target, LLTapHook hook, LLTapHookType type) {
  LLTap::hookmanager.add_hook(target, hook, type);
  return 1;
}

int lltap_register_hook_i(LLTapHookInfo* info) {
  LLTap::hookmanager.add_hook(info->target, info->hook, info->type);
  return 1;
}

void __lltap_inst_add_hook_target(void* addr, char* name) {
  LLTap::hookmanager.add_target(name, addr);
}

LLTapHook __lltap_inst_get_hook(void* addr, LLTapHookType type) {
  return LLTap::hookmanager.get_hook(addr, type);
}


int __lltap_inst_has_hooks(void* addr) {
  return LLTap::hookmanager.get_hook_bitmap(addr);
}

void lltap_deregister_hook(char* target, LLTapHookType type)
{
  LLTap::hookmanager.remove_hook(target, type);
}

}
