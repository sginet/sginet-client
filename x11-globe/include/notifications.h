#ifndef HEADER_X11GLOBE_NOTIF
#define HEADER_X11GLOBE_NOTIF

#define MAX_NOTIFICATIONS 5

typedef struct {
    char text[256];
    float timeLeft; /* seconds */
    int active;
} Notification;

void drawNotifications(int wWidth, int wHeight);
void addNotification(const char* msg, float duration);
int updateNotifications(float deltaTime);

#endif /* HEADER_X11GLOBE_NOTIF */
