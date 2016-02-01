/**
 * Implements the LockTree algorithm using LLTap hooks to tap into the pthread
 * API.
 *
 * Currently using these hooks you will get dot files for the locktrees of the
 * launched threads. Deadlock checking is not yet implemented.
 *
 * Warning: This implementation assumes that only one thread is launched at a
 * time!
 * So a proper "test driver" should do something like:
 * create(T1)
 * join(T1)
 * create(T2)
 * join(T2)
 */

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
#define THREAD_COUNT_MAX 2
LockTree* locktrees[THREAD_COUNT_MAX] = {0, };
pthread_t* threads[THREAD_COUNT_MAX] = {0, };
/*
 * OK so we cannot rely on pthread_self for thread identification... this is
 * bad :(
 *
 * Consider this program:
 *
#include <pthread.h>
#include <stdio.h>

void* thread1(void * a) {
  (void)a;
  puts("Thread1 launched");
  return NULL;
}
void* thread2(void * a) {
  (void)a;
  puts("Thread2 launched");
  return NULL;
}

int main() {
  pthread_t t1;
  pthread_t t2;

  // sequential
  pthread_create(&t1, NULL, thread1, NULL);
  pthread_join(t1, NULL);
  pthread_create(&t2, NULL, thread2, NULL);
  printf("t1 == t2 : %d\n", pthread_equal(t1, t2));
  pthread_join(t2, NULL);
  printf("t1 == t2 : %d\n", pthread_equal(t1, t2));

  // parallel
  pthread_create(&t1, NULL, thread1, NULL);
  pthread_create(&t2, NULL, thread2, NULL);
  printf("t1 == t2 : %d\n", pthread_equal(t1, t2));
  pthread_join(t1, NULL);
  pthread_join(t2, NULL);
  printf("t1 == t2 : %d\n", pthread_equal(t1, t2));

  return 0;
}
  *
  * It will output something I really didn't expect:
  *
Thread1 launched
Thread2 launched
t1 == t2 : 1
t1 == t2 : 1
Thread1 launched
t1 == t2 : 0
Thread2 launched
t1 == t2 : 0
  *
  * This means the equality (1) is given when the threads are launched
  * sequentially. Only the parallel running threads are not considered equal.
  *
  * What we actually need is a way to uniquely identify a thread for the whole
  * program lifetime...
  *
  * The quick and dirty way is to introduce a global counter, which is used
  * as the current thread index.
  * note that this is in no way thread safe, but should be fine for locktree
  * since only one of the threads runs at a time.
  *
  */
size_t current_thread_index = 0;


static void* reallocarray(void* arr, size_t nmemb, size_t size)
{
  size_t newsize;
  if (__builtin_umull_overflow(size, nmemb, &newsize)) {
    errno = EINVAL;
    return NULL;
  }
  if (arr == NULL) {
    return malloc(newsize);
  }
  return realloc(arr, newsize);
}


/// this function either succeeds or kills the process on failure
static void locktree_create_node(LockTree* lt, pthread_mutex_t* mutex)
{
  LockTreeNode* newltn = malloc(sizeof(LockTreeNode));
  if (newltn == NULL) {
    err(ENOMEM, "Cannot acquire new memory for LockTreeNode");
  }

  LockTreeNode** children = NULL;
  size_t childcount = 0;
  if (lt->current == NULL) {
    children = lt->children;
    childcount = ++lt->childcount;
  } else {
    children = lt->current->children;
    childcount = ++lt->current->childcount;
  }
  void* r = reallocarray(children, childcount, sizeof(LockTreeNode*));
  if (r == NULL) {
    err(ENOMEM, "Cannot resize LockTree(Node) children array");
  }
  children = r;
  if (lt->current == NULL) {
    lt->children = r;
  } else {
    lt->current->children = r;
  }

  // set values
  *newltn = (LockTreeNode){lt->current, NULL, 0, mutex};
  // append to children
  children[childcount-1] = newltn;
  // switch current to newly created node
  lt->current = newltn;
}


static void locktree_onunlock(LockTree* lt, pthread_mutex_t* mutex)
{
  if (lt->current != NULL) {
    if (lt->current->lock != mutex) {
      errx(254,
          "unlocking mutex at %p, which is not the current LockTree leaf",
          (void*)mutex);
    }
    lt->current = lt->current->parent;
  } else {
    warn("You can't unlock something that isn't locked (tried %p)",
         (void*)mutex);
  }
}


static void locktree_free(LockTree* lt) {
  // TODO: free the whole tree
  (void)lt;
}


static size_t get_thread_index(pthread_t t) {
  size_t i = current_thread_index;
  while (i < THREAD_COUNT_MAX
         && threads[i] != NULL
         && pthread_equal(t, *(threads[i]) != 0)) {
    i++;
  }
  if (i == THREAD_COUNT_MAX) {
    errx(255, "[LT] thread index %zu impossible", i);
  }
  return i;
}


static size_t get_current_thread_index() {
  return get_thread_index(pthread_self());
}


int create_hook(pthread_t *thread,
                const pthread_attr_t *attr,
                void *(*start_routine) (void *),
                void *arg) {

  pthread_mutex_lock(&locktreelock);

  pthread_t mythread;

  int retval = pthread_create(&mythread, attr, start_routine, arg);

  if (retval == 0) {
    size_t i = 0;
    i = get_thread_index(mythread);

    if (i >= THREAD_COUNT_MAX) {
      pthread_mutex_unlock(&locktreelock);
      errx(254, "Can't start more than %d threads with LockTree!", THREAD_COUNT_MAX);
    }

    warnx("[LT] Thread T%zu created", i);

    if (threads[i] == NULL) {
      threads[i] = malloc(sizeof(pthread_t));
      if (threads[i] == NULL) {
        pthread_mutex_unlock(&locktreelock);
        err(ENOMEM, "Failed to allocate storage for pthread_t id");
      }
      *threads[i] = mythread;
      warnx("[LT] allocatd pthread_t for T%zu", i);
    }

    if (locktrees[i] == NULL) {
      locktrees[i] = malloc(sizeof(LockTree));
      if (locktrees[i] == NULL) {
        pthread_mutex_unlock(&locktreelock);
        err(ENOMEM, "failed to allocate LockTree storage");
      }
      warnx("[LT] allocatd LockTree for T%zu", i);
    }

    warnx("[LT] Thread %zu - pthread_t at %p (was at %p) - LockTree at %p",
        i, (void*)threads[i], (void*)thread, (void*)locktrees[i]);

    *locktrees[i] = (LockTree){0, 0, 0};
  }

  *thread = mythread;

  pthread_mutex_unlock(&locktreelock);

  return retval;
}


void lock_hook(pthread_mutex_t** mutex) {
  pthread_mutex_lock(&locktreelock);

  size_t i = get_current_thread_index();
  warnx("[LT] lock_hook in T%zu", i);
  if (threads[i] == NULL || locktrees[i] == NULL) {
    pthread_mutex_unlock(&locktreelock);
    errx(254, "[LT] No LockTree found for this thread (T%zu)", i);
  }
  locktree_create_node(locktrees[i], *mutex);

  pthread_mutex_unlock(&locktreelock);
}


void unlock_hook(pthread_mutex_t** mutex) {
  pthread_mutex_lock(&locktreelock);

  size_t i = get_current_thread_index();
  warnx("[LT] unlock_hook in T%zu", i);
  if (threads[i] == NULL || locktrees[i] == NULL) {
    pthread_mutex_unlock(&locktreelock);
    errx(254, "[LT] No LockTree found for this thread (T%zu)", i);
  }
  locktree_onunlock(locktrees[i], *mutex);

  pthread_mutex_unlock(&locktreelock);
}


static void locktree_dotfile_visitor(LockTreeNode* node, FILE* f) {
  if (node == NULL || f == NULL) { return; }

  for (size_t i = 0; i < node->childcount; i++) {
    fprintf(f, "    \"%p\" -- \"%p\";\n", (void*)node->lock, (void*)node->children[i]->lock);
    locktree_dotfile_visitor(node->children[i], f);
  }
}

static void locktree_print_visitor(LockTreeNode* node, size_t indent) {
  if (node == NULL) { return; }

  for (size_t i = 0; i < node->childcount; i++) {
    for (size_t j = 0; j < indent; j++) {
      fputs("  ", stderr);
    }
    fprintf(stderr, "|- %p\n", (void*)node->lock);
    locktree_print_visitor(node->children[i], indent + 1);
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
      warnx("No locktree for thread %zu", i);
      continue;
    }
    fprintf(stderr, "Generating LockTree for thread %zu\n", i);
    char fname[] = "locktree_threadX.dot";
    snprintf(fname, sizeof(fname), "locktree_thread%zu.dot", i);
    FILE* f = fopen(fname, "w");
    if (f == NULL) {
      warn("Failed to open file '%s'", fname);
      continue;
    }
    fprintf(f, "graph locktree_thread%zu {\n", i);
    fprintf(stderr, "T%zu\n", i);

    if ((lt->childcount == 0 && lt->children != NULL) ||
        (lt->childcount > 0 && lt->children == NULL)) {
      errx(255, "inconsistent locktree state for LockTree at %p "
           "(childcount=%zu, children=%p)",
           (void*)lt, lt->childcount, (void*)lt->children);
    }

    for (size_t j = 0; j < lt->childcount; j++) {
      if (lt->children[j] != NULL) {
        fprintf(f, "    T%zu -- \"%p\";\n", i, (void*)lt->children[j]->lock);
        locktree_dotfile_visitor(lt->children[j], f);
        size_t indent = 1;
        locktree_print_visitor(lt->children[j], indent);
      } else {
        warnx("child %zu for LockTree(%zu, %p) is NULL", j, i, (void*)lt);
      }
    }

    fputs("}\n", f);
    fclose(f);

  }


  for (size_t i = 0; i < 2; ++i) {
    locktree_free(locktrees[i]);
  }

  pthread_mutex_unlock(&locktreelock);
}


void join_hook(int* retval, pthread_t thread, void** thread_retval) {
  (void) retval; (void) thread; (void) thread_retval;
  fprintf(stderr, "join of T%zu", current_thread_index);
  current_thread_index++;
}


LLTAP_HOOKSV my_hooks[] = {
  {"pthread_create", (LLTapHook) create_hook, LLTAP_REPLACE_HOOK},
  {"pthread_mutex_lock", (LLTapHook) lock_hook, LLTAP_PRE_HOOK},
  {"pthread_mutex_unlock", (LLTapHook) unlock_hook, LLTAP_PRE_HOOK},
  {"pthread_join", (LLTapHook) join_hook, LLTAP_POST_HOOK},
  LLTAP_HOOKSV_END,
};
LLTAP_REGISTER_HOOKS(my_hooks)
