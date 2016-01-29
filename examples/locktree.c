#include <liblltap.h>
#include <stdio.h>
#include <pthread.h>
#include <err.h>
#include <errno.h>
#include <stdbool.h>

typedef struct LockTreeNode LockTreeNode;
struct LockTreeNode {
  LockTreeNode* parent;
  LockTreeNode** children;
  size_t childcount;
  pthread_mutex_t* lock;
};

typedef struct LockTree LockTree;
struct LockTree {
  LockTreeNode* current;
  LockTreeNode** children;
  size_t childcount;
};

pthread_mutex_t locktreelock = PTHREAD_MUTEX_INITIALIZER;
pthread_t* threads[3] = {0, };
LockTree* locktrees[2] = {0, };


void* reallocarray(void* arr, size_t nmemb, size_t size)
{
  size_t newsize;
  if (__builtin_umull_overflow(size, nmemb, &newsize)) {
    errno = ENOMEM;
    return NULL;
  }
  return realloc(arr, newsize);
}


/// this function either succeeds or kills the process on failure
void locktree_create_node(LockTree* lt, pthread_mutex_t* mutex)
{
  LockTreeNode* newltn = malloc(sizeof(LockTreeNode));
  if (newltn == NULL) {
    err(ENOMEM, "Cannot acquire new memory for LockTreeNode");
  }

  LockTreeNode** children = NULL;
  size_t* childcount = NULL;
  if (lt->current == NULL) {
    children = lt->children;
    childcount = &lt->childcount;
  } else {
    children = lt->current->children;
    childcount = &lt->current->childcount;
  }
  (*childcount)++;
  void* r = reallocarray(children, *childcount, sizeof(LockTreeNode*));
  if (r == NULL) {
    err(ENOMEM, "Cannot acquire new memory");
  }
  if (lt->current == NULL) {
    lt->children = r;
  } else {
    lt->current->children = r;
  }

  *newltn = (LockTreeNode){lt->current, NULL, 0, mutex};
  lt->current = newltn;
}


void locktree_onunlock(LockTree* lt, pthread_mutex_t* mutex)
{
  if (lt->current != NULL) {
    if (lt->current->lock != mutex) {
      errx(254,
          "unlocking mutex at %p, which is not the current LockTree leaf",
          (void*)mutex);
    }
    lt->current = lt->current->parent;
  } else {
    errx(254,
        "You can't unlock something that isn't locked (tried %p)",
        (void*)mutex);
  }
}


void locktree_free(LockTree* lt) {
  // TODO: free the whole tree
}


void create_hook(int* retval,
                 pthread_t *thread,
                 const pthread_attr_t *attr,
                 void *(*start_routine) (void *),
                 void *arg) {
  // make compiler shut up about unused variables
  (void)attr; (void)start_routine; (void)arg; (void)retval;

  pthread_mutex_lock(&locktreelock);

  size_t i = 0;
  while (threads[i] != NULL && !pthread_equal(*thread, *threads[i])) {
    i++;
  }
  if (i >= 2) {
    errx(254, "Can't start more than 2 threads with LockTree!");
  }

  warnx("[LT] Thread %zu created\n", i);

  if (threads[i] == NULL) {
    threads[i] = malloc(sizeof(pthread_t));
    if (threads[i] == NULL) {
      err(ENOMEM, "Failed to allocate storage for pthread_t id");
    }
    *threads[i] = *thread;
  }

  if (locktrees[i] == NULL) {
    locktrees[i] = malloc(sizeof(LockTree));
    if (locktrees[i] == NULL) {
      err(ENOMEM, "failed to allocate LockTree storage");
    }
  }

  *locktrees[i] = (LockTree){0, 0, 0};


  pthread_mutex_unlock(&locktreelock);
}


void lock_hook(pthread_mutex_t** mutex) {
  pthread_mutex_lock(&locktreelock);

  pthread_t self = pthread_self();

  size_t i = 0;
  while (threads[i] != NULL && !pthread_equal(self, *threads[i])) {
    i++;
  }
  if (threads[i] == NULL || locktrees[i] == NULL) {
    errx(254, "No LockTree found for this thread");
  }

  locktree_create_node(locktrees[i], *mutex);

  pthread_mutex_unlock(&locktreelock);
}


void unlock_hook(pthread_mutex_t** mutex) {
  pthread_mutex_lock(&locktreelock);

  pthread_t self = pthread_self();

  size_t i = 0;
  while (threads[i] != NULL && !pthread_equal(self, *threads[i])) {
    i++;
  }
  if (threads[i] == NULL || locktrees[i] == NULL) {
    errx(254, "No LockTree found for this thread");
  }

  locktree_onunlock(locktrees[i], *mutex);

  pthread_mutex_unlock(&locktreelock);
}


static void locktree_dotfile_visitor(LockTreeNode* node, FILE* f) {
  if (node == NULL || f == NULL) { return; }

  for (size_t i = 0; i < node->childcount; i++) {
    fprintf(f, "    %p -- %p;\n", (void*)node->lock, (void*)node->children[i]->lock);
    locktree_dotfile_visitor(node->children[i], f);
  }
}


void
__attribute__((destructor))
locktree_process()
{
  pthread_mutex_lock(&locktreelock);

  fputs("LockTree: Processing\n", stderr);
  for (size_t i = 0; i < 2; ++i) {
    LockTree* lt = locktrees[i];
    if (lt == NULL) {
      continue;
    }
    fprintf(stderr, "Generating LockTree for thread %zu", i);
    char fname[] = "locktree_threadX.dot";
    snprintf(fname, sizeof(fname), "locktree_thread%zu.dot", i);
    FILE* f = fopen(fname, "w");
    if (f == NULL) {
      warn("Failed to open file '%s'", fname);
      continue;
    }
    fprintf(f, "graph locktree_thread%zu {\n", i);

    for (size_t j = 0; j < lt->childcount; j ++) {
      fprintf(f, "    T%zu -- %p;\n", i, (void*)lt->children[i]->lock);
      locktree_dotfile_visitor(lt->children[j], f);
    }

    fputs("}\n", f);
    fclose(f);

  }


  for (size_t i = 0; i < 2; ++i) {
    locktree_free(locktrees[i]);
  }

  pthread_mutex_unlock(&locktreelock);
}



LLTAP_HOOKSV my_hooks[] = {
  {"pthread_create", (LLTapHook) create_hook, LLTAP_POST_HOOK},
  {"pthread_mutex_lock", (LLTapHook) lock_hook, LLTAP_PRE_HOOK},
  {"pthread_mutex_unlock", (LLTapHook) unlock_hook, LLTAP_PRE_HOOK},
  LLTAP_HOOKSV_END,
};
LLTAP_REGISTER_HOOKS(my_hooks)
