use rand::RngCore;
use std::fmt;
use std::net::{Ipv4Addr, SocketAddrV4};

// Crockford base32
const ALPHABET: &[u8; 32] = b"0123456789ABCDEFGHJKMNPQRSTVWXYZ";
const KEY_BYTES: usize = 10;
const KEY_CHARS: usize = 16;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct RoomKey {
    pub addr: SocketAddrV4,
    pub secret: [u8; 4],
}

#[derive(Debug, PartialEq, Eq)]
pub enum KeyError {
    Length,
    BadChar(char),
}

impl fmt::Display for KeyError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            KeyError::Length => write!(f, "Room key should be 16 characters after RB-"),
            KeyError::BadChar(c) => write!(f, "Room key has an invalid character: {c}"),
        }
    }
}

impl std::error::Error for KeyError {}

impl RoomKey {
    pub fn generate(addr: SocketAddrV4) -> Self {
        let mut secret = [0u8; 4];
        rand::thread_rng().fill_bytes(&mut secret);
        Self { addr, secret }
    }

    fn to_bytes(self) -> [u8; KEY_BYTES] {
        let mut bytes = [0u8; KEY_BYTES];
        bytes[0..4].copy_from_slice(&self.addr.ip().octets());
        bytes[4..6].copy_from_slice(&self.addr.port().to_be_bytes());
        bytes[6..10].copy_from_slice(&self.secret);
        bytes
    }

    pub fn encode(&self) -> String {
        let body = encode_base32(&self.to_bytes());
        let groups: Vec<&str> = (0..KEY_CHARS).step_by(4).map(|i| &body[i..i + 4]).collect();
        format!("RB-{}", groups.join("-"))
    }

    pub fn parse(text: &str) -> Result<Self, KeyError> {
        let cleaned: String = text
            .chars()
            .filter(|c| c.is_ascii_alphanumeric())
            .map(|c| c.to_ascii_uppercase())
            .collect();
        let body = match cleaned.strip_prefix("RB") {
            Some(rest) if rest.len() == KEY_CHARS => rest,
            _ => cleaned.as_str(),
        };
        if body.len() != KEY_CHARS {
            return Err(KeyError::Length);
        }

        let bytes = decode_base32(body)?;
        let ip = Ipv4Addr::new(bytes[0], bytes[1], bytes[2], bytes[3]);
        let port = u16::from_be_bytes([bytes[4], bytes[5]]);
        let mut secret = [0u8; 4];
        secret.copy_from_slice(&bytes[6..10]);
        Ok(Self { addr: SocketAddrV4::new(ip, port), secret })
    }
}

impl fmt::Display for RoomKey {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str(&self.encode())
    }
}

fn encode_base32(bytes: &[u8]) -> String {
    let mut out = String::new();
    let mut buffer: u32 = 0;
    let mut bits = 0;
    for &byte in bytes {
        buffer = (buffer << 8) | byte as u32;
        bits += 8;
        while bits >= 5 {
            bits -= 5;
            out.push(ALPHABET[((buffer >> bits) & 31) as usize] as char);
        }
    }
    if bits > 0 {
        out.push(ALPHABET[((buffer << (5 - bits)) & 31) as usize] as char);
    }
    out
}

fn decode_base32(text: &str) -> Result<Vec<u8>, KeyError> {
    let mut out = Vec::new();
    let mut buffer: u32 = 0;
    let mut bits = 0;
    for c in text.chars() {
        buffer = (buffer << 5) | symbol_value(c)?;
        bits += 5;
        if bits >= 8 {
            bits -= 8;
            out.push((buffer >> bits) as u8);
        }
    }
    Ok(out)
}

fn symbol_value(c: char) -> Result<u32, KeyError> {
    // Lookalikes map to digits
    let c = match c {
        'O' => '0',
        'I' | 'L' => '1',
        other => other,
    };
    ALPHABET
        .iter()
        .position(|&a| a as char == c)
        .map(|i| i as u32)
        .ok_or(KeyError::BadChar(c))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sample() -> RoomKey {
        RoomKey { addr: SocketAddrV4::new(Ipv4Addr::new(192, 168, 1, 177), 47300), secret: [9, 200, 31, 77] }
    }

    #[test]
    fn round_trip() {
        let key = sample();
        let text = key.encode();
        assert_eq!(text.len(), 22);
        assert_eq!(RoomKey::parse(&text).unwrap(), key);
    }

    #[test]
    fn forgiving_input() {
        let key = sample();
        let messy = key.encode().to_lowercase().replace('-', " ");
        assert_eq!(RoomKey::parse(&messy).unwrap(), key);
        let bare = key.encode().trim_start_matches("RB-").to_string();
        assert_eq!(RoomKey::parse(&bare).unwrap(), key);
    }

    #[test]
    fn rejects_bad_keys() {
        assert_eq!(RoomKey::parse("RB-1234"), Err(KeyError::Length));
        assert_eq!(RoomKey::parse("RB-UUUU-UUUU-UUUU-UUUU"), Err(KeyError::BadChar('U')));
    }

    #[test]
    fn keys_are_unique() {
        let addr = sample().addr;
        assert_ne!(RoomKey::generate(addr).encode(), RoomKey::generate(addr).encode());
    }
}
