#pragma once

#include <furi.h>

#include "walk.h"

/* Harold talks back: tells the desktop app what he is doing, over the same
 * data channel the app writes to (helper_protocol.h, Flipper -> PC). The main
 * loop calls talk_tick with what it sees; talk queues a message when
 * something changes. Packets go out through link_send_data. */

typedef struct {
    bool awake;
    bool walking;
    bool waiting; /* holding back while the user works */
    uint8_t mode;
    uint8_t connection;
    bool playing; /* a planned walk or the circle, not a wander */
    uint32_t quack_until; /* the home screen's quack deadline */
} TalkSeen;

typedef struct {
    TalkSeen last;
    uint32_t state_at;
    bool started;
} Talk;

void talk_init(Talk* talk);

/* Once per loop, with the app lock held. `path` is the walk that plays when
 * `seen.playing` turns on. */
void talk_tick(Talk* talk, const TalkSeen* seen, const WalkPath* path, uint32_t now);

/* Duckwalk is closing. */
void talk_closing(void);

/* Packing, exposed for the host tests. Each fills a 63-byte packet. */
void talk_pack_state(uint8_t* p, const TalkSeen* seen);
/* Steps from `first` on; returns how many went in (0 when none are left). */
uint8_t talk_pack_walk(uint8_t* p, const WalkPath* path, uint16_t first);
