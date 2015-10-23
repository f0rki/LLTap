#include <stdio.h>

void say_hello(char* name) {
  printf("Hello %s!\n", name);
}

int main(int argc, char* argv[]) {
  say_hello("World");
}
