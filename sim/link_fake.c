/* A connection that is whatever the scene says it is. */

#include "sim.h"

SimLink sim_link = {.kind = LinkUsb, .connected = true, .on_air = true};

void link_start(LinkKind kind) {
    sim_link.kind = kind;
}

void link_stop(void) {
}

LinkKind link_kind(void) {
    return sim_link.kind;
}

bool link_is_connected(void) {
    return sim_link.connected;
}

const char* link_status(void) {
    if(sim_link.kind == LinkUsb) return !sim_link.on_air ? "USB busy" : sim_link.connected ? "USB" : "no USB";
    if(sim_link.connected) return "BT";
    return sim_link.on_air ? "BT pairing" : "BT starting";
}

bool link_off(void) {
    return !sim_link.on_air;
}

const char* link_bt_name(void) {
    return "Harold Rgudalap";
}

void link_bt_forget(void) {
}

bool link_mouse_move(int8_t dx, int8_t dy) {
    UNUSED(dx);
    UNUSED(dy);
    return sim_link.connected;
}

bool link_mouse_click(uint8_t button) {
    UNUSED(button);
    return sim_link.connected;
}

bool link_mouse_scroll(int8_t wheel) {
    UNUSED(wheel);
    return sim_link.connected;
}

bool link_consumer_tap(uint16_t usage) {
    UNUSED(usage);
    return sim_link.connected;
}

void link_set_data_callback(LinkDataCallback callback, void* ctx) {
    UNUSED(callback);
    UNUSED(ctx);
}

void link_flush(void) {
}

bool link_busy(void) {
    return false;
}
