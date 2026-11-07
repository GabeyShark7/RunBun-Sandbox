#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RbRoom RbRoom;

enum RbEvent {
    RB_EVENT_NONE = 0,
    RB_EVENT_HOSTING = 1,
    RB_EVENT_JOINED = 2, // you host peers
    RB_EVENT_PEER_JOINED = 3,
    RB_EVENT_PEER_LEFT = 4,
    RB_EVENT_CHAT = 5,
    RB_EVENT_PEERS = 6,
    RB_EVENT_INFO = 7,
    RB_EVENT_CLOSED = 8
};

RbRoom* rb_room_host(const char* name, uint16_t port);
// Null on a bad key
RbRoom* rb_room_join(const char* name, const char* key, char* error, size_t error_size);
void rb_room_close(RbRoom* room);
void rb_room_chat(RbRoom* room, const char* text);
void rb_room_kick(RbRoom* room, const char* name);
// Lists split on newlines
int rb_room_poll(RbRoom* room, char* buffer, size_t size);

#ifdef __cplusplus
}
#endif
