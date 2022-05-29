#include "core.h"

int main(int argc, char *argv[])
{
  core_init();

  while (fsm_cycle_state());
}
