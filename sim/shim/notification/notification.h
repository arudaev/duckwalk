#pragma once

#include <furi.h>

typedef struct NotificationApp NotificationApp;
typedef struct {
    int type;
} NotificationMessage;
typedef const NotificationMessage* NotificationSequence[];

#define RECORD_NOTIFICATION "notification"

/* Sound and vibration don't exist in the simulator. */
void notification_message(NotificationApp* app, const NotificationSequence* sequence);
