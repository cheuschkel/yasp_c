#include <stdint.h>

#include "cmd_def.h"
#include "../yasp.h"
#include "../serial_HAL.h"

void init_application()
{
    init_gpio_commands();
    init_i2c_commands();
    yasp_init();
}

void run_application()
{
    serialService();
}