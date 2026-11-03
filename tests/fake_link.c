/* A connection for host tests: always connected, counts what is sent. */

#include "link.h"

int fake_link_moves = 0;

void link_set_data_callback(LinkDataCallback callback, void* ctx) {
    (void)callback;
    (void)ctx;
}

bool link_is_connected(void) {
    return true;
}

bool link_mouse_move(int8_t dx, int8_t dy) {
    (void)dx;
    (void)dy;
    fake_link_moves++;
    return true;
}
