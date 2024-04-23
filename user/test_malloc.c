#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  test_malloc();
  exit(0);
}