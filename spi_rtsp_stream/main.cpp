#include "app/app_spi.h"

int main() {
    app_init();
    while (true)
        app_tick();
    return 0;
}
