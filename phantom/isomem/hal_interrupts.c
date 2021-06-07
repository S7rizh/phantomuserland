#include <hal.h>
#include "genode_misc.h"

void hal_cli(void)
{
    _stub_print();
}

void hal_sti(void)
{
    _stub_print();
}

int hal_is_sti(void)
{
    _stub_print();
    return 0;
}

int hal_save_cli(void)
{
    _stub_print();
    return 1; // From unix hal
}
