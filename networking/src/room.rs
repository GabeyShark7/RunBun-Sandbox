use crate::protocol::{clean_text, join_proof, verify_proof, Msg, Reply, MAX_CHAT, MAX_NAME, PROTOCOL};
use crate::room_key::RoomKey;
use futures::StreamExt;
use libp2p::request_response::{self, ProtocolSupport, ResponseChannel};
use libp2p::swarm::{NetworkBehaviour, SwarmEvent};
use libp2p::{multiaddr::Protocol, noise, tcp, yamux, Multiaddr, PeerId, StreamProtocol, Swarm};
use std::collections::{HashMap, HashSet};
use std::future::Future;
use std::net::{Ipv4Addr, SocketAddr, SocketAddrV4, UdpSocket};
use std::sync::mpsc;
use std::thread::JoinHandle;
use std::time::{Duration, Instant};
use tokio::sync::mpsc::{unbounded_channel, UnboundedReceiver, UnboundedSender};

pub const DEFAULT_PORT: u16 = 47300;
const JOIN_TIMEOUT: Duration = Duration::from_secs(10);

#[derive(Debug, Clone)]
pub enum Command {
    Chat(String),
    Kick(String),
    Peers,
    Leave,
}

#[derive(Debug, Clone)]
pub enum Event {
    Hosting { key: RoomKey },
    Joined { host: String, you: String, peers: Vec<String> },
    PeerJoined(String),
    PeerLeft(String),
    Chat { from: String, text: String },
    Peers(Vec<String>),
    Info(String),
    Closed(String),
}

#[derive(NetworkBehaviour)]
struct Behaviour {
    room: request_response::cbor::Behaviour<Msg, Reply>,
}

pub struct RoomHandle {
    commands: UnboundedSender<Command>,
    events: mpsc::Receiver<Event>,
    thread: Option<JoinHandle<()>>,
}

#[derive(Clone)]
struct Out(mpsc::Sender<Event>);

impl Out {
    fn emit(&self, event: Event) {
        let _ = self.0.send(event);
    }
}

impl RoomHandle {
    pub fn host(name: &str, port: u16) -> Self {
        let name = name.to_string();
        Self::spawn(move |commands, out| async move {
            let reason = run_host(name, port, commands, &out).await.unwrap_or_else(|e| format!("Error: {e}"));
            out.emit(Event::Closed(reason));
        })
    }

    pub fn join(name: &str, key: RoomKey) -> Self {
        let name = name.to_string();
        Self::spawn(move |commands, out| async move {
            let reason = run_join(name, key, commands, &out).await.unwrap_or_else(|e| format!("Error: {e}"));
            out.emit(Event::Closed(reason));
        })
    }

    fn spawn<F, Fut>(task: F) -> Self
    where
        F: FnOnce(UnboundedReceiver<Command>, Out) -> Fut + Send + 'static,
        Fut: Future<Output = ()>,
    {
        let (command_tx, command_rx) = unbounded_channel();
        let (event_tx, event_rx) = mpsc::channel();
        // Runs off the UI thread
        let thread = std::thread::spawn(move || {
            let runtime = tokio::runtime::Builder::new_current_thread()
                .enable_all()
                .build()
                .expect("tokio runtime");
            runtime.block_on(task(command_rx, Out(event_tx)));
        });
        Self { commands: command_tx, events: event_rx, thread: Some(thread) }
    }

    pub fn send(&self, command: Command) {
        let _ = self.commands.send(command);
    }

    pub fn sender(&self) -> UnboundedSender<Command> {
        self.commands.clone()
    }

    pub fn poll(&self) -> Option<Event> {
        self.events.try_recv().ok()
    }

    pub fn wait(&self) -> Option<Event> {
        self.events.recv().ok()
    }
}

impl Drop for RoomHandle {
    fn drop(&mut self) {
        let _ = self.commands.send(Command::Leave);
        if let Some(thread) = self.thread.take() {
            let _ = thread.join();
        }
    }
}

fn build_swarm() -> Result<Swarm<Behaviour>, String> {
    let swarm = libp2p::SwarmBuilder::with_new_identity()
        .with_tokio()
        .with_tcp(tcp::Config::default(), noise::Config::new, yamux::Config::default)
        .map_err(|e| e.to_string())?
        .with_behaviour(|_| Behaviour {
            room: request_response::cbor::Behaviour::new(
                [(StreamProtocol::new(PROTOCOL), ProtocolSupport::Full)],
                request_response::Config::default(),
            ),
        })
        .map_err(|e| e.to_string())?
        .with_swarm_config(|c| c.with_idle_connection_timeout(Duration::from_secs(60 * 60 * 24)))
        .build();
    Ok(swarm)
}

fn tcp_addr(ip: Ipv4Addr, port: u16) -> Multiaddr {
    Multiaddr::empty().with(Protocol::Ip4(ip)).with(Protocol::Tcp(port))
}

// No packets are sent
fn lan_ip() -> Option<Ipv4Addr> {
    let socket = UdpSocket::bind("0.0.0.0:0").ok()?;
    socket.connect("8.8.8.8:80").ok()?;
    match socket.local_addr().ok()? {
        SocketAddr::V4(addr) if !addr.ip().is_loopback() => Some(*addr.ip()),
        _ => None,
    }
}

fn reply(swarm: &mut Swarm<Behaviour>, channel: ResponseChannel<Reply>, response: Reply) {
    let _ = swarm.behaviour_mut().room.send_response(channel, response);
}

struct HostState {
    name: String,
    secret: [u8; 4],
    peers: HashMap<PeerId, String>,
    pending: HashMap<PeerId, Instant>,
    denied: HashSet<PeerId>,
}

impl HostState {
    fn names(&self) -> Vec<String> {
        let mut names = vec![format!("{} (host)", self.name)];
        names.extend(self.peers.values().cloned());
        names
    }

    fn unique_name(&self, wanted: &str) -> String {
        let cleaned = clean_text(wanted, MAX_NAME);
        let base = if cleaned.is_empty() { "Guest" } else { cleaned.as_str() };
        let taken = |n: &str| n == self.name || self.peers.values().any(|p| p == n);
        let mut name = base.to_string();
        let mut i = 2;
        while taken(&name) {
            name = format!("{base} {i}");
            i += 1;
        }
        name
    }
}

fn broadcast(swarm: &mut Swarm<Behaviour>, peers: &HashMap<PeerId, String>, skip: Option<PeerId>, msg: Msg) {
    for peer in peers.keys().filter(|p| Some(**p) != skip) {
        swarm.behaviour_mut().room.send_request(peer, msg.clone());
    }
}

async fn run_host(name: String, port: u16, mut commands: UnboundedReceiver<Command>, out: &Out) -> Result<String, String> {
    let mut swarm = build_swarm()?;
    let me = *swarm.local_peer_id();

    // Else any free port
    if swarm.listen_on(tcp_addr(Ipv4Addr::UNSPECIFIED, port)).is_err() {
        swarm.listen_on(tcp_addr(Ipv4Addr::UNSPECIFIED, 0)).map_err(|e| e.to_string())?;
    }

    let mut state = HostState {
        name: match clean_text(&name, MAX_NAME) { n if n.is_empty() => "Host".into(), n => n },
        secret: [0; 4],
        peers: HashMap::new(),
        pending: HashMap::new(),
        denied: HashSet::new(),
    };
    let mut announced = false;
    let mut tick = tokio::time::interval(Duration::from_secs(1));

    loop {
        tokio::select! {
            command = commands.recv() => match command {
                None | Some(Command::Leave) => return Ok("You closed the room".into()),
                Some(Command::Chat(text)) => {
                    let msg = Msg::Chat { from: state.name.clone(), text: clean_text(&text, MAX_CHAT) };
                    broadcast(&mut swarm, &state.peers, None, msg);
                }
                Some(Command::Kick(target)) => {
                    match state.peers.iter().find(|(_, n)| **n == target).map(|(p, _)| *p) {
                        Some(peer) => { let _ = swarm.disconnect_peer_id(peer); }
                        None => out.emit(Event::Info(format!("No one named {target} is in the room"))),
                    }
                }
                Some(Command::Peers) => out.emit(Event::Peers(state.names())),
            },

            _ = tick.tick() => {
                // Drop silent connections
                let now = Instant::now();
                let stale: Vec<PeerId> = state.pending.iter()
                    .filter(|(_, t)| now.duration_since(**t) > JOIN_TIMEOUT)
                    .map(|(p, _)| *p).collect();
                for peer in stale {
                    state.pending.remove(&peer);
                    let _ = swarm.disconnect_peer_id(peer);
                }
            }

            event = swarm.select_next_some() => match event {
                SwarmEvent::NewListenAddr { address, .. } if !announced => {
                    let Some(Protocol::Tcp(bound)) = address.iter().nth(1) else { continue };
                    let ip = lan_ip().unwrap_or(Ipv4Addr::LOCALHOST);
                    let key = RoomKey::generate(SocketAddrV4::new(ip, bound));
                    state.secret = key.secret;
                    announced = true;
                    out.emit(Event::Hosting { key });
                }
                SwarmEvent::ConnectionEstablished { peer_id, .. } => {
                    if !state.peers.contains_key(&peer_id) {
                        state.pending.entry(peer_id).or_insert_with(Instant::now);
                    }
                }
                SwarmEvent::ConnectionClosed { peer_id, num_established: 0, .. } => {
                    state.pending.remove(&peer_id);
                    state.denied.remove(&peer_id);
                    if let Some(left) = state.peers.remove(&peer_id) {
                        broadcast(&mut swarm, &state.peers, None, Msg::PeerLeft { name: left.clone() });
                        out.emit(Event::PeerLeft(left));
                    }
                }
                SwarmEvent::Behaviour(BehaviourEvent::Room(event)) => match event {
                    request_response::Event::Message {
                        peer,
                        message: request_response::Message::Request { request, channel, .. },
                        ..
                    } => handle_host_request(&mut swarm, &mut state, me, peer, request, channel, out),
                    request_response::Event::ResponseSent { peer, .. } if state.denied.contains(&peer) => {
                        let _ = swarm.disconnect_peer_id(peer);
                    }
                    _ => {}
                },
                _ => {}
            },
        }
    }
}

fn handle_host_request(
    swarm: &mut Swarm<Behaviour>,
    state: &mut HostState,
    me: PeerId,
    peer: PeerId,
    request: Msg,
    channel: ResponseChannel<Reply>,
    out: &Out,
) {
    match request {
        Msg::Join { name, proof } => {
            if state.peers.contains_key(&peer) {
                reply(swarm, channel, Reply::Ok);
            } else if verify_proof(&state.secret, &me, &peer, &proof) {
                let name = state.unique_name(&name);
                let peers = state.peers.values().cloned().collect();
                reply(swarm, channel, Reply::Welcome { host: state.name.clone(), you: name.clone(), peers });
                broadcast(swarm, &state.peers, None, Msg::PeerJoined { name: name.clone() });
                state.pending.remove(&peer);
                state.peers.insert(peer, name.clone());
                out.emit(Event::PeerJoined(name));
            } else {
                state.denied.insert(peer);
                reply(swarm, channel, Reply::Denied { reason: "Wrong room key".into() });
                out.emit(Event::Info("Turned away a join with a wrong key".into()));
            }
        }
        Msg::Chat { text, .. } => {
            // Host stamps the sender
            let Some(from) = state.peers.get(&peer).cloned() else {
                reply(swarm, channel, Reply::Denied { reason: "Join the room first".into() });
                return;
            };
            let text = clean_text(&text, MAX_CHAT);
            if text.is_empty() {
                reply(swarm, channel, Reply::Ok);
                return;
            }
            reply(swarm, channel, Reply::Ok);
            broadcast(swarm, &state.peers, Some(peer), Msg::Chat { from: from.clone(), text: text.clone() });
            out.emit(Event::Chat { from, text });
        }
        _ => reply(swarm, channel, Reply::Ok),
    }
}

async fn run_join(name: String, key: RoomKey, mut commands: UnboundedReceiver<Command>, out: &Out) -> Result<String, String> {
    let mut swarm = build_swarm()?;
    let me = *swarm.local_peer_id();
    swarm.dial(tcp_addr(*key.addr.ip(), key.addr.port())).map_err(|e| e.to_string())?;
    out.emit(Event::Info(format!("Connecting to {}", key.addr)));

    let started = Instant::now();
    let mut host: Option<PeerId> = None;
    let mut joined = false;
    let mut peers: Vec<String> = Vec::new();
    let mut tick = tokio::time::interval(Duration::from_secs(1));

    loop {
        tokio::select! {
            command = commands.recv() => match command {
                None | Some(Command::Leave) => return Ok("You left the room".into()),
                Some(Command::Chat(text)) => match host {
                    Some(h) if joined => {
                        swarm.behaviour_mut().room.send_request(&h, Msg::Chat { from: name.clone(), text });
                    }
                    _ => out.emit(Event::Info("Not in the room yet".into())),
                },
                Some(Command::Kick(_)) => out.emit(Event::Info("Only the host can kick".into())),
                Some(Command::Peers) => out.emit(Event::Peers(peers.clone())),
            },

            _ = tick.tick() => {
                if !joined && started.elapsed() > JOIN_TIMEOUT {
                    return Ok("Timed out joining the room".into());
                }
            }

            event = swarm.select_next_some() => match event {
                SwarmEvent::ConnectionEstablished { peer_id, endpoint, .. } if host.is_none() && endpoint.is_dialer() => {
                    host = Some(peer_id);
                    let proof = join_proof(&key.secret, &peer_id, &me);
                    swarm.behaviour_mut().room.send_request(&peer_id, Msg::Join { name: name.clone(), proof });
                }
                SwarmEvent::OutgoingConnectionError { error, .. } if host.is_none() => {
                    return Ok(format!("Could not reach the room: {error}"));
                }
                SwarmEvent::ConnectionClosed { peer_id, num_established: 0, .. } if Some(peer_id) == host => {
                    return Ok(if joined { "The host closed the room".into() } else { "The host hung up".into() });
                }
                SwarmEvent::Behaviour(BehaviourEvent::Room(request_response::Event::Message { peer, message, .. })) => {
                    if Some(peer) != host { continue; }
                    match message {
                        request_response::Message::Response { response, .. } => match response {
                            Reply::Welcome { host: host_name, you, peers: others } => {
                                joined = true;
                                peers = std::iter::once(format!("{host_name} (host)")).chain(others.iter().cloned()).collect();
                                peers.push(format!("{you} (you)"));
                                out.emit(Event::Joined { host: host_name, you, peers: others });
                            }
                            Reply::Denied { reason } => return Ok(format!("Join refused: {reason}")),
                            Reply::Ok => {}
                        },
                        request_response::Message::Request { request, channel, .. } => {
                            reply(&mut swarm, channel, Reply::Ok);
                            match request {
                                Msg::Chat { from, text } => out.emit(Event::Chat { from, text }),
                                Msg::PeerJoined { name: who } => {
                                    peers.push(who.clone());
                                    out.emit(Event::PeerJoined(who));
                                }
                                Msg::PeerLeft { name: who } => {
                                    peers.retain(|p| *p != who);
                                    out.emit(Event::PeerLeft(who));
                                }
                                Msg::Join { .. } => {}
                            }
                        }
                    }
                }
                _ => {}
            },
        }
    }
}
