#include "main_loop.hpp"
#include "options.hpp"
#include "ui.hpp"


Options global_options;


int main(void)
{
    init_ui();

    main_loop();

    return 0;
}
