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
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum Reply {
    Welcome { host: String, you: String, peers: Vec<String> },
    Denied { reason: String },
    Ok,
}

pub const MAX_NAME: usize = 32;
pub const MAX_CHAT: usize = 500;

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
    fn proof_checks_secret_and_peers() {
        let host = PeerId::random();
        let joiner = PeerId::random();
        let proof = join_proof(&[1, 2, 3, 4], &host, &joiner);
        assert!(verify_proof(&[1, 2, 3, 4], &host, &joiner, &proof));
        assert!(!verify_proof(&[1, 2, 3, 5], &host, &joiner, &proof));
        assert!(!verify_proof(&[1, 2, 3, 4], &host, &PeerId::random(), &proof));
    }
}
