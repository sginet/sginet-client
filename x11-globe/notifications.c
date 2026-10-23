#include "include/globe.h"
#include "include/notifications.h"
#include "include/gl_utility.h"

static Notification notifications[MAX_NOTIFICATIONS];

/* Adds a notification with message and duration (seconds) */
void addNotification(const char* msg, float duration) {
    int i;
    for (i = 0; i < MAX_NOTIFICATIONS; i++) {
        if (!notifications[i].active) {
            strncpy(notifications[i].text, msg, sizeof(notifications[i].text) - 1);
            notifications[i].text[sizeof(notifications[i].text) - 1] = '\0';
            notifications[i].timeLeft = duration;
            notifications[i].active = 1;
            break;
        }
    }
}

/* Update notifications timers, remove expired */
int updateNotifications(float deltaTime) {
    int i;
    int hasChanges = 0;
    for (i = 0; i < MAX_NOTIFICATIONS; i++) {
        if (notifications[i].active) {
            notifications[i].timeLeft -= deltaTime;
            if (notifications[i].timeLeft <= 0.0f) {
                notifications[i].active = 0;
                hasChanges = 1;
            }
        }
    }
    return hasChanges;
}

/* Draw notifications as boxes with text */
void drawNotifications(int wWidth, int wHeight) {
    int i;
    float boxHeight = 30.0f;
    float boxWidth = wWidth - 40;
    float startY = wHeight - boxHeight - 10;

    for (i = 0; i < MAX_NOTIFICATIONS; i++) {
        if (notifications[i].active) {
            float y = startY - i * (boxHeight + 10);
            /* Draw box background */
            draw_rectangle(20, y, boxWidth, boxHeight, 0.3f, 0.3f, 0.3f, 0.7f);

            /* Draw text in white */
            glColor3f(1.0f, 1.0f, 1.0f);
            drawString(notifications[i].text, 30, (int)(y + 10));
        }
    }
}
