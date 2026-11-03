/* A connection for host tests: always connected, counts what is sent and
 * keeps track of where the pointer is. */

#include "link.h"

int fake_link_moves = 0;
int fake_link_x = 0;
int fake_link_y = 0;

void link_set_data_callback(LinkDataCallback callback, void* ctx) {
    (void)callback;
    (void)ctx;
}

bool link_is_connected(void) {
    return true;
}

bool link_mouse_move(int8_t dx, int8_t dy) {
    fake_link_moves++;
    fake_link_x += dx;
    fake_link_y += dy;
    return true;
}
