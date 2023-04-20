#include <stdio.h>
#include <stdlib.h>

#include "fileaccess_lib.h"

int system_mount()
{
  return system("bash/mountfs");
}

int system_umount()
{
  return system("bash/umountfs");
}