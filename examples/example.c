#include <stdio.h>

char* msgs[] = {"hello", "world", "hello", "universe"};

void example()
{
  puts("example called");
}

int print_multi(int i, char** m)
{
  int r = 0;
  int ret = 0;
  for (int j = 0; j < i; j++) {
    r = puts(m[j]);
    if (r == EOF) {
      break;
    }
    ret++;
  }
  return ret;
}

int main()
{
  example();
  return print_multi(2, msgs);
}
