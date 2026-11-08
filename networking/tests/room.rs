use runbun_net::{Command, Event, RoomHandle, RoomKey};
use std::time::{Duration, Instant};

fn next(room: &RoomHandle, matches: impl Fn(&Event) -> bool) -> Event {
    let deadline = Instant::now() + Duration::from_secs(15);
    while Instant::now() < deadline {
        match room.poll() {
            Some(event) if matches(&event) => return event,
            Some(_) => {}
            None => std::thread::sleep(Duration::from_millis(20)),
        }
    }
    panic!("timed out waiting for event");
}

fn open_room(name: &str, port: u16) -> (RoomHandle, RoomKey) {
    let host = RoomHandle::host(name, port);
    match next(&host, |e| matches!(e, Event::Hosting { .. })) {
        Event::Hosting { key } => (host, key),
        _ => unreachable!(),
    }
}

#[test]
fn join_chat_and_leave() {
    let (host, key) = open_room("Host", 47401);
    let guest = RoomHandle::join("Guest", key);

    next(&guest, |e| matches!(e, Event::Joined { .. }));
    next(&host, |e| matches!(e, Event::PeerJoined(n) if n == "Guest"));

    guest.send(Command::Chat("hi from guest".into()));
    next(&host, |e| matches!(e, Event::Chat { from, text } if from == "Guest" && text == "hi from guest"));

    host.send(Command::Chat("hi from host".into()));
    next(&guest, |e| matches!(e, Event::Chat { from, .. } if from == "Host"));

    // Same name gets a suffix
    let second = RoomHandle::join("Guest", key);
    next(&host, |e| matches!(e, Event::PeerJoined(n) if n == "Guest 2"));
    next(&guest, |e| matches!(e, Event::PeerJoined(n) if n == "Guest 2"));

    drop(second);
    next(&host, |e| matches!(e, Event::PeerLeft(n) if n == "Guest 2"));

    drop(host);
    next(&guest, |e| matches!(e, Event::Closed(_)));
}

#[test]
fn wrong_secret_is_refused() {
    let (host, mut key) = open_room("Host", 47402);
    key.secret[0] ^= 0xFF;
    let intruder = RoomHandle::join("Intruder", key);
    match next(&intruder, |e| matches!(e, Event::Closed(_))) {
        Event::Closed(reason) => assert!(reason.contains("Wrong room key"), "{reason}"),
        _ => unreachable!(),
    }
    host.send(Command::Peers);
    match next(&host, |e| matches!(e, Event::Peers(_))) {
        Event::Peers(list) => assert_eq!(list.len(), 1),
        _ => unreachable!(),
    }
}

#[test]
fn scene_data_flows_in_order() {
    let (host, key) = open_room("Host", 47403);
    let guest = RoomHandle::join("Guest", key);
    next(&guest, |e| matches!(e, Event::Joined { .. }));
    next(&host, |e| matches!(e, Event::PeerJoined(n) if n == "Guest"));

    // Big enough to split
    let big: Vec<u8> = (0..900_000u32).map(|i| (i % 251) as u8).collect();
    host.send(Command::Scene { data: big.clone(), to: Some("Guest".into()), skip: None, from: None });
    match next(&guest, |e| matches!(e, Event::Scene { .. })) {
        Event::Scene { from, data } => {
            assert_eq!(from, "Host");
            assert_eq!(data, big);
        }
        _ => unreachable!(),
    }

    for i in 0..20u8 {
        guest.send(Command::Scene { data: vec![i; 10], to: None, skip: None, from: None });
    }
    for i in 0..20u8 {
        match next(&host, |e| matches!(e, Event::Scene { .. })) {
            Event::Scene { from, data } => {
                assert_eq!(from, "Guest");
                assert_eq!(data, vec![i; 10]);
            }
            _ => unreachable!(),
        }
    }

    host.send(Command::Scene { data: vec![7; 5], to: None, skip: None, from: Some("Guest".into()) });
    match next(&guest, |e| matches!(e, Event::Scene { .. })) {
        Event::Scene { from, .. } => assert_eq!(from, "Guest"),
        _ => unreachable!(),
    }
}

#[test]
fn full_and_locked_rooms_refuse() {
    let (host, key) = open_room("Host", 47404);
    host.send(Command::Limits { max_people: 2, open: true });
    let first = RoomHandle::join("First", key);
    next(&host, |e| matches!(e, Event::PeerJoined(n) if n == "First"));
    let second = RoomHandle::join("Second", key);
    match next(&second, |e| matches!(e, Event::Closed(_))) {
        Event::Closed(reason) => assert!(reason.contains("full"), "{reason}"),
        _ => unreachable!(),
    }
    host.send(Command::Limits { max_people: 8, open: false });
    let third = RoomHandle::join("Third", key);
    match next(&third, |e| matches!(e, Event::Closed(_))) {
        Event::Closed(reason) => assert!(reason.contains("locked"), "{reason}"),
        _ => unreachable!(),
    }
    drop(first);
}
