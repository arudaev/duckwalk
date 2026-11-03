/* Sound and vibration are silent in the simulator. */

#include <notification/notification_messages.h>

const NotificationMessage message_vibro_on = {0};
const NotificationMessage message_vibro_off = {0};
const NotificationMessage message_sound_off = {0};
const NotificationMessage message_delay_50 = {0};
const NotificationMessage message_delay_100 = {0};
const NotificationMessage message_note_d4 = {0};
const NotificationMessage message_note_f4 = {0};

void notification_message(NotificationApp* app, const NotificationSequence* sequence) {
    UNUSED(app);
    UNUSED(sequence);
}
