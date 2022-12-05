# TinyFramePlusPlus

A [TinyFrame](https://github.com/MightyPork/TinyFrame) port to C++

This repo contains an onject oriented, template based implementation of TinyFrame, which is fully compatible with the other TinyFrame versions.

## TinyFramePlusPlus compared to the TinyFrame (C)

- TinyFrame is defined by a class instead of a struct
- No `extern "C"`
- All elements defined within a namespace
- Individual Configurations
  - Configurable using template arguments instead of defines
  - Each object has its own configuration
  - Each object has its own callbacks (`WriteImpl, Error, ClaimTx, ReleaseTx`)
- Additinal CRC implementations (see `CKSUM_t`)
  - table-based (big, fast)
  - algorithm-based (small, slow)
- Enhanced error information (see `ErrorMsg_t`)
- Dynamic or static allocation (C++)
  - dynamic: `new TinyFrame_n::TinyFrame<...>(...)`
  - static: `TinyFrame_n::tinyFrameCreate_Static<...>()`

## The other Ports

TinyFrame has been ported to mutiple languages:

- The reference C implementation - [MightyPork/TinyFrame](https://github.com/MightyPork/TinyFrame)
- C++ port - [marcelnabeck/TinyFramePlusPlus](https://github.com/marcelnabeck/TinyFramePlusPlus)
- Python port - [MightyPork/PonyFrame](https://github.com/MightyPork/PonyFrame)
- Rust port - [cpsdqs/tinyframe-rs](https://github.com/cpsdqs/tinyframe-rs)
- JavaScript port - [cpsdqs/tinyframe-js](https://github.com/cpsdqs/tinyframe-js)

Please note most of the ports are experimental and may exhibit various bugs or missing
features. Testers are welcome :)

## Functional overview

The basic functionality of TinyFrame is explained here. For particlars, such as the
API functions, it's recommended to read the doc comments in the header file.

### Structure of a frame

Each frame consists of a header and a payload. Both parts can be protected by a checksum,
ensuring a frame with a malformed header (e.g. with a corrupted length field) or a corrupted
payload is rejected.

The frame header contains a frame ID and a message type. Frame ID is incremented with each
new message. The highest bit of the ID field is fixed to 1 and 0 for the two peers,
avoiding a conflict.

Frame ID can be re-used in a response to tie the two messages together. Values of the
type field are user defined.

All fields in the frame have a configurable size. By changing a field in the config
file, such as `LEN_BYTES` (1, 2 or 4), the library seamlessly switches between `uint8_t`,
`uint16_t` and `uint32_t` for all functions working with the field.

```text
,-----+-----+-----+------+------------+- - - -+-------------,
| SOF | ID  | LEN | TYPE | HEAD_CKSUM | DATA  | DATA_CKSUM  |
| 0-1 | 1-4 | 1-4 | 1-4  | 0-4        | ...   | 0-4         | <- size (bytes)
'-----+-----+-----+------+------------+- - - -+-------------'

SOF ......... start of frame, usually 0x01 (optional, configurable)
ID  ......... the frame ID (MSb is the peer bit)
LEN ......... number of data bytes in the frame
TYPE ........ message type (used to run Type Listeners, pick any values you like)
HEAD_CKSUM .. header checksum

DATA ........ LEN bytes of data
DATA_CKSUM .. data checksum (left out if LEN is 0)
```

### Message listeners

TinyFrame is based on the concept of message listeners. A listener is a callback function
waiting for a particular message Type or ID to be received.

There are 3 listener types, in the order of precedence:

- **ID listeners** - waiting for a response
- **Type listeners** - waiting for a message of the given Type field
- **Generic listeners** - fallback

ID listeners can be registered automatically when sending a message. All listeners can
also be registered and removed manually.

ID listeners are used to receive the response to a request. When registerign an ID
listener, it's possible to attach custom user data to it that will be made available to
the listener callback. This data (`void *`) can be any kind of application context
variable.

ID listeners can be assigned a timeout. When a listener expires, before it's removed,
the callback is fired with NULL payload data in order to let the user free any
attached userdata. This happens only if the userdata is not `nullptr`.

Listener callbacks return values of the `Result` enum:

- `Result::CLOSE` - message accepted, remove the listener
- `Result::STAY` - message accepted, stay registered
- `Result::RENEW` - sameas `Result::STAY`, but the ID listener's timeout is renewed
- `Result::NEXT` - message NOT accepted, keep the listener and pass the message to the next
              listener capable of handling it.

### Data buffers, multi-part frames

TinyFrame uses two data buffers: a small transmit buffer and a larger receive buffer.
The transmit buffer is used to prepare bytes to send, either all at once, or in a
circular fashion if the buffer is not large enough. The buffer must only contain the entire
frame header, so e.g. 32 bytes should be sufficient for short messages.

Using the `*_Multipart()` sending functions, it's further possible to split the frame
header and payload to multiple function calls, allowing the applciation to e.g. generate
the payload on-the-fly.

In contrast to the transmit buffer, the receive buffer must be large enough to contain
an entire frame. This is because the final checksum must be verified before the frame
is handled.

If frames larger than the possible receive buffer size are required (e.g. in embedded
systems with small RAM), it's recommended to implement a multi-message transport mechanism
at a higher level and send the data in chunks.
