#include <stdio.h>
#include <pthread.h>
#include <err.h>
#include <string.h>

pthread_mutex_t lock_1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock_2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock_3 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock_4 = PTHREAD_MUTEX_INITIALIZER;

void* thread_one(void* arg)
{
  (void)arg;
  pthread_mutex_lock(&lock_1);
  pthread_mutex_lock(&lock_2);
  pthread_mutex_lock(&lock_3);
  pthread_mutex_lock(&lock_4);
  pthread_mutex_unlock(&lock_4);
  pthread_mutex_unlock(&lock_3);
  pthread_mutex_unlock(&lock_2);
  pthread_mutex_unlock(&lock_1);
  return NULL;
}

void* thread_two(void* arg)
{
  (void)arg;
  pthread_mutex_lock(&lock_1);
  pthread_mutex_lock(&lock_2);
  pthread_mutex_lock(&lock_4);
  pthread_mutex_lock(&lock_3);
  pthread_mutex_unlock(&lock_3);
  pthread_mutex_unlock(&lock_4);
  pthread_mutex_unlock(&lock_2);
  pthread_mutex_unlock(&lock_1);
  return NULL;
}


int main()
{
  puts("Starting T0");
  pthread_t t0;
  int r = pthread_create(&t0, NULL, thread_one, NULL);
  if (r != 0) {
    err(r, "Failed to start thread: %s", strerror(r));
  }
  puts("Joining T0");
  pthread_join(t0, NULL);

  puts("Starting T1");
  pthread_t t1;
  r = pthread_create(&t1, NULL, thread_two, NULL);
  if (r != 0) {
    err(r, "Failed to start thread: %s", strerror(r));
  }
  puts("Joining T1");
  pthread_join(t1, NULL);

  if (pthread_equal(t1, t0) != 0) {
    puts("For some reason t0 == t1");
    return 1;
  }

  puts("Bye");
  return 0;
}
