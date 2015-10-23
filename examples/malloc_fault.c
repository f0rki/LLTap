#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>


int lowerall(size_t count, char* inputstrs[])
{
  char* strings[count-1];
  for (size_t i = 0; i < count; ++i) {
    size_t len = strlen(inputstrs[i]) + 1;
    // the return value of malloc isn't checked here.
    strings[i] = malloc(len);
    for (size_t j = 0; j < len; j++) {
      strings[i][j] = tolower(inputstrs[i][j]);
    }
    puts(strings[i]);
  }
  return count;
}


int main(void)
{
  char* inputstrs[] = {"HELLO", "WoRlD!!!!!", "foo", "bar", "bAz"};
  return lowerall(sizeof(inputstrs), inputstrs);
}
