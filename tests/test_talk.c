/* Harold talking back: the packets Duckwalk sends the desktop app. */

#include "check.h"
#include "helper_protocol.h"
#include "talk.h"

#include <string.h>

extern uint8_t fake_link_data[32][63];
extern int fake_link_data_count;

static int count_type(uint8_t type) {
    int n = 0;
    for(int i = 0; i < fake_link_data_count; i++) n += fake_link_data[i][2] == type;
    return n;
}

int main(void) {
    uint32_t t = 100000;
    shim_tick_set(t);
    static WalkPath path;

    /* A walk of 60 steps: 3 packets, each step relative to the one before. */
    path.count = 60;
    for(int i = 0; i < 60; i++) path.points[i] = (WalkPoint){(int16_t)(i * 2), (int16_t)(-i)};
    uint8_t p[HELPER_PACKET_LEN];
    CHECK(talk_pack_walk(p, &path, 0) == HELPER_WALK_STEPS, "first piece not full");
    CHECK(p[0] == HELPER_MAGIC && p[1] == HELPER_VERSION && p[2] == HELPER_MSG_WALK, "walk header");
    CHECK(helper_get_u16(&p[3]) == 60 && helper_get_u16(&p[5]) == 0 && p[7] == HELPER_WALK_STEPS, "walk counts");
    CHECK(p[8] == WALK_STEP_MS, "step time %d", p[8]);
    CHECK((int8_t)p[9] == 0 && (int8_t)p[10] == 0, "first step is from the start");
    CHECK((int8_t)p[11] == 2 && (int8_t)p[12] == -1, "second step (%d, %d)", (int8_t)p[11], (int8_t)p[12]);
    CHECK(talk_pack_walk(p, &path, 54) == 6, "last piece holds the rest");
    CHECK(helper_get_u16(&p[5]) == 54, "last piece starts at 54");
    CHECK(talk_pack_walk(p, &path, 60) == 0, "nothing past the end");

    /* Big jumps are clamped to a signed byte. */
    path.count = 2;
    path.points[0] = (WalkPoint){300, -300};
    talk_pack_walk(p, &path, 0);
    CHECK((int8_t)p[9] == 127 && (int8_t)p[10] == -127, "jump not clamped");

    /* State: sent at once, on a change, and every 2 s; nothing in between. */
    static Talk talk;
    talk_init(&talk);
    TalkSeen seen = {.awake = true, .mode = 2, .connection = 1, .quack_until = t};
    talk_tick(&talk, &seen, &path, t);
    CHECK(fake_link_data_count == 1 && fake_link_data[0][2] == HELPER_MSG_STATE, "no state at start");
    CHECK(fake_link_data[0][3] == HELPER_STATE_AWAKE && fake_link_data[0][4] == 2 && fake_link_data[0][5] == 1, "state data");
    talk_tick(&talk, &seen, &path, t += 100);
    CHECK(fake_link_data_count == 1, "state repeated without a change");
    seen.waiting = true;
    talk_tick(&talk, &seen, &path, t += 100);
    CHECK(fake_link_data_count == 2 && fake_link_data[1][3] == (HELPER_STATE_AWAKE | HELPER_STATE_WAITING), "change not sent");
    talk_tick(&talk, &seen, &path, t += HELPER_STATE_EVERY_MS);
    CHECK(fake_link_data_count == 3, "no heartbeat after 2 s");

    /* A walk starting sends its path once, before the state change. */
    path.count = 60;
    fake_link_data_count = 0;
    seen.playing = seen.walking = true;
    talk_tick(&talk, &seen, &path, t += 20);
    CHECK(count_type(HELPER_MSG_WALK) == 3, "%d walk packets", count_type(HELPER_MSG_WALK));
    talk_tick(&talk, &seen, &path, t += 20);
    CHECK(count_type(HELPER_MSG_WALK) == 3, "walk sent twice");

    /* A quack: once, when its deadline moves into the future. */
    shim_tick_set(t);
    seen.quack_until = t + 600;
    talk_tick(&talk, &seen, &path, t += 20);
    talk_tick(&talk, &seen, &path, t += 20);
    CHECK(count_type(HELPER_MSG_QUACK) == 1, "%d quacks", count_type(HELPER_MSG_QUACK));

    /* Closing. */
    talk_closing();
    CHECK(fake_link_data[fake_link_data_count - 1][2] == HELPER_MSG_CLOSING, "no closing packet");

    return check_result("talk");
}
