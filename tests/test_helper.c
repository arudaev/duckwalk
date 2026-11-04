/* The PC helper's packets, and Harold letting go of the pointer while the
 * user works. */

#include "check.h"
#include "helper.h"
#include "helper_protocol.h"
#include "keep_awake.h"

#include <furi_hal_random.h>

extern int fake_link_moves;

static void packet(uint8_t* p, uint32_t idle_ms) {
    memset(p, 0, HELPER_PACKET_LEN);
    p[0] = HELPER_MAGIC;
    p[1] = HELPER_VERSION;
    p[2] = HELPER_MSG_IDLE;
    helper_put_u32(&p[3], idle_ms);
}

int main(void) {
    uint8_t p[HELPER_PACKET_LEN];
    uint32_t t = 100000;
    shim_tick_set(t);

    CHECK(!helper_present(), "present before any packet");

    /* Not ours: wrong magic, version or type. */
    packet(p, 0);
    p[0] = 'X';
    CHECK(!helper_receive(p, t), "wrong magic accepted");
    packet(p, 0);
    p[1] = HELPER_VERSION + 1;
    CHECK(!helper_receive(p, t), "wrong version accepted");
    packet(p, 0);
    p[2] = 0x7F;
    CHECK(!helper_receive(p, t), "unknown message accepted");
    CHECK(!helper_present(), "a rejected packet made the helper present");

    /* The user just typed. */
    packet(p, 0);
    CHECK(helper_receive(p, t), "idle packet rejected");
    CHECK(helper_present(), "not present after a packet");
    CHECK(helper_user_active(30000), "not active right after input");

    /* 29 s later, still within the hold, with heartbeats coming in. */
    t += 29000;
    shim_tick_set(t);
    packet(p, 29000);
    helper_receive(p, t);
    CHECK(helper_user_active(30000), "inactive 29 s after input");

    /* 31 s after the input, the hold is over. */
    t += 2000;
    shim_tick_set(t);
    packet(p, 31000);
    helper_receive(p, t);
    CHECK(!helper_user_active(30000), "still active 31 s after input");

    /* An idle time longer than the Flipper's uptime is just "long ago". */
    packet(p, 0xFFFFFFFF);
    helper_receive(p, t);
    CHECK(!helper_user_active(30000), "huge idle time counted as active");

    /* The helper goes quiet: gone after the timeout. */
    t += HELPER_TIMEOUT_MS + 1;
    shim_tick_set(t);
    CHECK(!helper_present(), "present after the timeout");
    CHECK(!helper_user_active(30000), "active without a helper");

    /* Still gone a month later, past the 24.8 days where tick differences
     * wrap. */
    bool back = false;
    for(int hour = 0; hour < 30 * 24; hour++) {
        shim_tick_set(t += 3600000);
        back |= helper_present();
    }
    CHECK(!back, "a helper gone for weeks came back by itself");

    /* Keep-awake: a walk in progress stops when the user starts working,
     * nothing starts while they work, and walks resume after. */
    static KeepAwake ka;
    shim_random_seed(3);
    keep_awake_init(&ka, 30, 60);
    keep_awake_set_mode(&ka, KeepAwakeWalk);
    keep_awake_walk_now(&ka);
    shim_tick_set(t += 200);
    keep_awake_tick(&ka);
    CHECK(keep_awake_is_walking(&ka), "walk didn't start");

    keep_awake_set_user_active(&ka, true);
    CHECK(!keep_awake_is_walking(&ka), "walk kept going while the user works");
    int moves = fake_link_moves;
    for(int i = 0; i < 120 * 50; i++) {
        shim_tick_set(t += 20);
        keep_awake_tick(&ka);
    }
    CHECK(fake_link_moves == moves, "moved the pointer %d times while the user worked", fake_link_moves - moves);

    keep_awake_set_user_active(&ka, false);
    bool walked = false;
    for(int i = 0; i < 70 * 50 && !walked; i++) {
        shim_tick_set(t += 20);
        keep_awake_tick(&ka);
        walked = keep_awake_is_walking(&ka);
    }
    CHECK(walked, "no walk within 70 s after the user stopped");

    return check_result("helper");
}
