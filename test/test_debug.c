#include <string.h>
#include <stdio.h>

#include "debug.h"

int main(int argc, char **argv)
{
  DEBUG("Hello debug\n");
  LOG("Hello log\n");
  WARNING("Hello warning\n");
}