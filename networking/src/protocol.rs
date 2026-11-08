use hmac::{Hmac, Mac};
use libp2p::PeerId;
use serde::{Deserialize, Serialize};
use sha2::Sha256;

pub const PROTOCOL: &str = "/runbun/room/1";

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum Msg {
    Join { name: String, proof: Vec<u8> },
    Chat { from: String, text: String },
    PeerJoined { name: String },
    PeerLeft { name: String },
    Scene { from: String, id: u64, part: u32, total: u32, size: u64, #[serde(with = "packed")] data: Vec<u8> },
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum Reply {
    Welcome { host: String, you: String, peers: Vec<String> },
    Denied { reason: String },
    Ok,
}

// One blob not a list
mod packed {
    use serde::de::{SeqAccess, Visitor};
    use serde::{Deserializer, Serializer};
    use std::fmt;

    pub fn serialize<S: Serializer>(data: &Vec<u8>, s: S) -> Result<S::Ok, S::Error> {
        s.serialize_bytes(data)
    }

    struct Bytes;

    impl<'de> Visitor<'de> for Bytes {
        type Value = Vec<u8>;
        fn expecting(&self, f: &mut fmt::Formatter) -> fmt::Result {
            f.write_str("bytes")
        }
        fn visit_bytes<E>(self, v: &[u8]) -> Result<Vec<u8>, E> {
            Ok(v.to_vec())
        }
        fn visit_byte_buf<E>(self, v: Vec<u8>) -> Result<Vec<u8>, E> {
            Ok(v)
        }
        fn visit_seq<A: SeqAccess<'de>>(self, mut seq: A) -> Result<Vec<u8>, A::Error> {
            let mut out = Vec::with_capacity(seq.size_hint().unwrap_or(0).min(1 << 20));
            while let Some(b) = seq.next_element()? {
                out.push(b);
            }
            Ok(out)
        }
    }

    pub fn deserialize<'de, D: Deserializer<'de>>(d: D) -> Result<Vec<u8>, D::Error> {
        d.deserialize_byte_buf(Bytes)
    }
}

pub const MAX_NAME: usize = 32;
pub const MAX_CHAT: usize = 500;
pub const SCENE_CHUNK: usize = 192 * 1024;
pub const MAX_SCENE: u64 = 512 * 1024 * 1024;

// One line and capped
pub fn clean_text(text: &str, max_chars: usize) -> String {
    text.chars().map(|c| if c.is_control() { ' ' } else { c }).take(max_chars).collect::<String>().trim().to_string()
}

// Bound to both peer ids
fn proof_mac(secret: &[u8; 4], host: &PeerId, joiner: &PeerId) -> Hmac<Sha256> {
    let mut mac = Hmac::<Sha256>::new_from_slice(secret).expect("hmac takes any key size");
    mac.update(&host.to_bytes());
    mac.update(&joiner.to_bytes());
    mac
}

pub fn join_proof(secret: &[u8; 4], host: &PeerId, joiner: &PeerId) -> Vec<u8> {
    proof_mac(secret, host, joiner).finalize().into_bytes().to_vec()
}

pub fn verify_proof(secret: &[u8; 4], host: &PeerId, joiner: &PeerId, proof: &[u8]) -> bool {
    proof_mac(secret, host, joiner).verify_slice(proof).is_ok()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn scene_data_is_one_blob() {
        let msg = Msg::Scene { from: "a".into(), id: 1, part: 0, total: 1, size: 1000, data: vec![200; 1000] };
        let mut out = Vec::new();
        cbor4ii::serde::to_writer(&mut out, &msg).unwrap();
        assert!(out.len() < 1100);
        let back: Msg = cbor4ii::serde::from_slice(&out).unwrap();
        match back {
            Msg::Scene { data, .. } => assert_eq!(data, vec![200; 1000]),
            _ => panic!(),
        }
    }

    #[test]
    fn proof_checks_secret_and_peers() {
        let host = PeerId::random();
        let joiner = PeerId::random();
        let proof = join_proof(&[1, 2, 3, 4], &host, &joiner);
        assert!(verify_proof(&[1, 2, 3, 4], &host, &joiner, &proof));
        assert!(!verify_proof(&[1, 2, 3, 5], &host, &joiner, &proof));
        assert!(!verify_proof(&[1, 2, 3, 4], &host, &PeerId::random(), &proof));
    }
}
