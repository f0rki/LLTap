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
  pthread_t t1;
  int r = pthread_create(&t1, NULL, thread_one, NULL);
  if (r != 0) {
    errx(r, "Failed to start thread: %s", strerror(r));
  }
  pthread_join(t1, NULL);

  pthread_t t2;
  r = pthread_create(&t2, NULL, thread_one, NULL);
  if (r != 0) {
    errx(r, "Failed to start thread: %s", strerror(r));
  }
  pthread_join(t2, NULL);

  return 0;
}
