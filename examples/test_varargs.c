#include <stdio.h>
#include <stdarg.h>

int fu(int argc, ...)
{
  va_list ap;
  va_start(ap, argc);
  for (int i = 0; i < argc; ++i) {
    char* s = va_arg(ap, char*);
    puts(s);
  }
  va_end(ap);
  return 0;
}


int main()
{
  int i = fu(2, "one", "two");
  int j = fu(3, "three", "four", "five");
  /*printf("main - %d %d\n", i, j);        */
  return i + j;
}
