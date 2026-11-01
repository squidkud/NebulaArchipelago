// SPDX-License-Identifier: GPL-3.0-only
// A minimal WebSocket client (RFC 6455) for the Archipelago client. Windows (Winsock) and POSIX over a plain TCP socket
// (ws://); on Windows also over TLS (wss://, Windows' own SChannel, certificates checked against the system's roots).
// Elsewhere a wss:// address is refused. Text messages only; ping, pong and close are answered here.
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace galaxy::net {

// ---- The pieces, for tests ----

// SHA-1 of `data`, as 20 raw bytes.
[[nodiscard]] std::string sha1(std::string_view data);
[[nodiscard]] std::string base64_encode(std::string_view data);
// Sec-WebSocket-Accept for a Sec-WebSocket-Key.
[[nodiscard]] std::string websocket_accept_key(std::string_view client_key);

struct WebSocketUrl {
    std::string host;
    std::string port;  // "38281"; "80" when the address names none
    std::string path;  // "/" at least
    bool secure = false;
    bool explicit_scheme = false;  // the address said ws:// or wss:// (otherwise either may do)
};
// "host:port", "ws://host:port/path" or "wss://host:port"; nullopt when it names no host.
[[nodiscard]] std::optional<WebSocketUrl> parse_websocket_url(std::string_view address);

// One frame, 4 mask bytes when `mask` is not null (a client's frames are masked).
[[nodiscard]] std::string encode_frame(std::uint8_t opcode, std::string_view payload, const std::uint8_t* mask);

struct Frame {
    bool fin = true;
    std::uint8_t opcode = 0;
    std::string payload;  // unmasked
};

// Reads frames out of the bytes that arrive, in pieces.
class FrameDecoder {
public:
    void feed(const char* data, std::size_t size);
    // The next whole frame; false when more bytes are needed. `error()` becomes true for a frame that cannot be read
    // (a payload over the limit, reserved bits).
    [[nodiscard]] bool next(Frame& out);
    [[nodiscard]] bool error() const noexcept { return error_; }

private:
    std::string buffer_;
    bool error_ = false;
};

// ---- The connection ----

struct TlsSession;  // a TLS session (Windows only)

class WebSocket {
public:
    enum class Event { Timeout, Message, Closed };

    WebSocket();
    WebSocket(const WebSocket&) = delete;
    WebSocket& operator=(const WebSocket&) = delete;
    ~WebSocket();

    // Connects and upgrades; false with `error` set when it fails (also for wss:// where TLS is not built in).
    [[nodiscard]] bool connect(const WebSocketUrl& url, int timeout_ms, std::string& error);
    [[nodiscard]] bool is_open() const noexcept;
    // One text message; false when the connection is gone.
    [[nodiscard]] bool send_text(std::string_view text);
    // Waits up to `timeout_ms` for a whole text message (fragments are joined). Pings are answered meanwhile; a close
    // frame or a broken connection returns Closed.
    [[nodiscard]] Event receive(std::string& message, int timeout_ms);
    void close();
    // Milliseconds since the server last sent anything (a ping or pong counts); used to notice a connection that died
    // without a close (Wi-Fi dropped, computer slept).
    [[nodiscard]] long long idle_ms() const noexcept;
    // A ping frame; false when the connection is gone. The server's pong then counts as activity.
    [[nodiscard]] bool send_ping();

    // Whether wss:// can be used in this build.
    [[nodiscard]] static bool tls_available() noexcept;

private:
    [[nodiscard]] bool send_all(const char* data, std::size_t size);
    [[nodiscard]] bool raw_send(const char* data, std::size_t size);
    // Up to `capacity` bytes of what the server sent (decrypted): the count; 0 when the connection closed, -1 on an
    // error; `timed_out` when nothing came within `timeout_ms`.
    [[nodiscard]] long read_some(char* buffer, std::size_t capacity, int timeout_ms, bool& timed_out);
    [[nodiscard]] bool send_frame(std::uint8_t opcode, std::string_view payload);

    std::unique_ptr<TlsSession> tls_;
    std::intptr_t socket_ = -1;
    FrameDecoder decoder_;
    std::string partial_;  // the message being joined from fragments
    bool open_ = false;
    std::chrono::steady_clock::time_point last_activity_ = std::chrono::steady_clock::now();
};

}  // namespace galaxy::net
