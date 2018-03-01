#include <unistd.h>
#include <stdio.h>

int main()
{
  // first test the helloworld system call
  int rc = syscall(326);

  if (rc == -1)
    printf("helloworld system call: failure! :(\n");
  else
    printf("helloworld system call: success! :)\n");

  // then test the simple_add system call
  int result;
  rc = syscall(327, 10, 7, &result);

  if (rc == -1)
    printf("simple_add system call: failure! :(\n");
  else
    printf("simple_add system call: 10 + 7 = %d\n", result);
}
