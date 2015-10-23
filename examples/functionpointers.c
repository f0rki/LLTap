#include <stdlib.h>
#include <stdio.h>

int (*transform)(char*) = NULL;

int to_lower(char* s)
{
  int i = 0;
  while (*s != '\0') {
    if (*s > 'A' && *s < 'Z') {
      *s += 32;
      i++;
    }
    s++;
  }
  return i;
}

int to_upper(char* s)
{
  int i = 0;
  while (*s != '\0') {
    if (*s > 'a' && *s < 'z') {
      *s -= 32;
      i++;
    }
    s++;
  }
  return i;
}

int main()
{
  char str[] = "Hello, Welcome To The Jungle!";
  char str2[] = "Hello, Welcome To The Jungle!";

  // calling directly
  to_lower(str2);
  puts(str2);
  to_upper(str2);
  puts(str2);

  // calling indirectly
  transform = to_lower;
  transform(str);
  puts(str);
  transform = to_upper;
  transform(str);
  puts(str);

  return 0;
}
