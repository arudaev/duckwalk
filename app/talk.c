#include "talk.h"
#include "helper_protocol.h"
#include "link.h"
#include "util.h"

static void header(uint8_t* p, uint8_t type) {
    memset(p, 0, HELPER_PACKET_LEN);
    p[0] = HELPER_MAGIC;
    p[1] = HELPER_VERSION;
    p[2] = type;
}

static int8_t clamp8(int32_t v) {
    return v > 127 ? 127 : v < -127 ? -127 : (int8_t)v;
}

void talk_pack_state(uint8_t* p, const TalkSeen* seen) {
    header(p, HELPER_MSG_STATE);
    p[3] = (seen->awake ? HELPER_STATE_AWAKE : 0) | (seen->walking ? HELPER_STATE_WALKING : 0) |
           (seen->waiting ? HELPER_STATE_WAITING : 0);
    p[4] = seen->mode;
    p[5] = seen->connection;
}

uint8_t talk_pack_walk(uint8_t* p, const WalkPath* path, uint16_t first) {
    if(first >= path->count) return 0;
    uint16_t n = path->count - first;
    if(n > HELPER_WALK_STEPS) n = HELPER_WALK_STEPS;
    header(p, HELPER_MSG_WALK);
    helper_put_u16(&p[3], path->count);
    helper_put_u16(&p[5], first);
    p[7] = (uint8_t)n;
    p[8] = WALK_STEP_MS;
    for(uint16_t i = 0; i < n; i++) {
        uint16_t at = first + i;
        WalkPoint from = at ? path->points[at - 1] : (WalkPoint){0, 0};
        WalkPoint to = path->points[at];
        p[HELPER_WALK_HEADER + i * 2] = (uint8_t)clamp8(to.x - from.x);
        p[HELPER_WALK_HEADER + i * 2 + 1] = (uint8_t)clamp8(to.y - from.y);
    }
    return (uint8_t)n;
}

static void send_simple(uint8_t type) {
    uint8_t p[HELPER_PACKET_LEN];
    header(p, type);
    link_send_data(p);
}

void talk_init(Talk* talk) {
    memset(talk, 0, sizeof(*talk));
}

void talk_tick(Talk* talk, const TalkSeen* seen, const WalkPath* path, uint32_t now) {
    uint8_t p[HELPER_PACKET_LEN];
    const TalkSeen* last = &talk->last;
    bool changed = !talk->started || seen->awake != last->awake || seen->walking != last->walking ||
                   seen->waiting != last->waiting || seen->mode != last->mode ||
                   seen->connection != last->connection;

    /* A walk starting: its whole path, so the app can show it as it plays. */
    if(talk->started && seen->playing && !last->playing) {
        for(uint16_t first = 0; talk_pack_walk(p, path, first); first += HELPER_WALK_STEPS) {
            link_send_data(p);
        }
    }
    if(talk->started && seen->quack_until != last->quack_until && tick_before(seen->quack_until)) {
        send_simple(HELPER_MSG_QUACK);
    }
    if(changed || now - talk->state_at >= furi_ms_to_ticks(HELPER_STATE_EVERY_MS)) {
        talk_pack_state(p, seen);
        if(link_send_data(p)) talk->state_at = now;
    }
    talk->last = *seen;
    talk->started = true;
}

void talk_closing(void) {
    send_simple(HELPER_MSG_CLOSING);
}
