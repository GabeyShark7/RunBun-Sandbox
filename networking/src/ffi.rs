use crate::room::{Event, RoomHandle};
use crate::room_key::RoomKey;
use crate::Command;
use std::ffi::{c_char, CStr};

// Same ids as the C header
const EVENT_NONE: i32 = 0;
const EVENT_HOSTING: i32 = 1;
const EVENT_JOINED: i32 = 2;
const EVENT_PEER_JOINED: i32 = 3;
const EVENT_PEER_LEFT: i32 = 4;
const EVENT_CHAT: i32 = 5;
const EVENT_PEERS: i32 = 6;
const EVENT_INFO: i32 = 7;
const EVENT_CLOSED: i32 = 8;

unsafe fn read_str(text: *const c_char) -> String {
    if text.is_null() {
        return String::new();
    }
    CStr::from_ptr(text).to_string_lossy().into_owned()
}

// Truncates on a char edge
unsafe fn write_str(text: &str, buffer: *mut c_char, size: usize) {
    if buffer.is_null() || size == 0 {
        return;
    }
    let mut end = text.len().min(size - 1);
    while !text.is_char_boundary(end) {
        end -= 1;
    }
    std::ptr::copy_nonoverlapping(text.as_ptr(), buffer as *mut u8, end);
    *buffer.add(end) = 0;
}

#[no_mangle]
pub unsafe extern "C" fn rb_room_host(name: *const c_char, port: u16) -> *mut RoomHandle {
    Box::into_raw(Box::new(RoomHandle::host(&read_str(name), port)))
}

#[no_mangle]
pub unsafe extern "C" fn rb_room_join(name: *const c_char, key: *const c_char, error: *mut c_char, error_size: usize) -> *mut RoomHandle {
    match RoomKey::parse(&read_str(key)) {
        Ok(key) => Box::into_raw(Box::new(RoomHandle::join(&read_str(name), key))),
        Err(e) => {
            write_str(&e.to_string(), error, error_size);
            std::ptr::null_mut()
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn rb_room_close(room: *mut RoomHandle) {
    if !room.is_null() {
        drop(Box::from_raw(room));
    }
}

#[no_mangle]
pub unsafe extern "C" fn rb_room_chat(room: *mut RoomHandle, text: *const c_char) {
    if let Some(room) = room.as_ref() {
        room.send(Command::Chat(read_str(text)));
    }
}

#[no_mangle]
pub unsafe extern "C" fn rb_room_kick(room: *mut RoomHandle, name: *const c_char) {
    if let Some(room) = room.as_ref() {
        room.send(Command::Kick(read_str(name)));
    }
}

#[no_mangle]
pub unsafe extern "C" fn rb_room_poll(room: *mut RoomHandle, buffer: *mut c_char, size: usize) -> i32 {
    let Some(event) = room.as_ref().and_then(RoomHandle::poll) else {
        return EVENT_NONE;
    };
    // Lists use newlines
    let (kind, text) = match event {
        Event::Hosting { key } => (EVENT_HOSTING, key.encode()),
        Event::Joined { host, you, peers } => (EVENT_JOINED, [you, host].into_iter().chain(peers).collect::<Vec<_>>().join("\n")),
        Event::PeerJoined(name) => (EVENT_PEER_JOINED, name),
        Event::PeerLeft(name) => (EVENT_PEER_LEFT, name),
        Event::Chat { from, text } => (EVENT_CHAT, format!("{from}\n{text}")),
        Event::Peers(list) => (EVENT_PEERS, list.join("\n")),
        Event::Info(text) => (EVENT_INFO, text),
        Event::Closed(reason) => (EVENT_CLOSED, reason),
    };
    write_str(&text, buffer, size);
    kind
}
