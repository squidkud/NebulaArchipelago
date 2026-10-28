// SPDX-License-Identifier: GPL-3.0-only
#include "galaxy/websocket_client.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <random>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wincrypt.h>
#ifndef SECURITY_WIN32
#define SECURITY_WIN32
#endif
#include <security.h>
#include <schannel.h>
#ifdef _MSC_VER
#pragma comment(lib, "secur32.lib")
#endif
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#endif

namespace galaxy::net {

namespace {

constexpr std::size_t kMaxPayload = 64u * 1024u * 1024u;  // the largest message taken
constexpr std::size_t kMaxHandshake = 16u * 1024u;

// ---- SHA-1 ----

std::uint32_t rotl(std::uint32_t value, unsigned bits) {
    return (value << bits) | (value >> (32u - bits));
}

// ---- Sockets, one set of names for Winsock and POSIX ----

#if defined(_WIN32)
using Socket = SOCKET;
constexpr Socket kBadSocket = INVALID_SOCKET;

void ensure_sockets() {
    static const bool started = [] {
        WSADATA data{};
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    (void)started;
}
int close_socket(Socket s) { return closesocket(s); }
bool set_blocking(Socket s, bool blocking) {
    u_long mode = blocking ? 0u : 1u;
    return ioctlsocket(s, FIONBIO, &mode) == 0;
}
bool connect_in_progress() {
    const int error = WSAGetLastError();
    return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS;
}
void set_send_timeout(Socket s, int ms) {
    const DWORD value = static_cast<DWORD>(ms);
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&value), sizeof(value));
}
#else
using Socket = int;
constexpr Socket kBadSocket = -1;

void ensure_sockets() {}
int close_socket(Socket s) { return ::close(s); }
bool set_blocking(Socket s, bool blocking) {
    const int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(s, F_SETFL, blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK)) == 0;
}
bool connect_in_progress() {
    return errno == EINPROGRESS || errno == EINTR;
}
void set_send_timeout(Socket s, int ms) {
    timeval value{};
    value.tv_sec = ms / 1000;
    value.tv_usec = (ms % 1000) * 1000;
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &value, sizeof(value));
}
#endif

Socket to_socket(std::intptr_t value) {
    return static_cast<Socket>(value);
}
std::intptr_t from_socket(Socket value) {
    return static_cast<std::intptr_t>(value);
}

// 1 when the socket can be read (or written), 0 when `ms` pass first, -1 on an error.
int wait_socket(Socket s, bool for_write, int ms) {
    if (ms < 0) ms = 0;
#if !defined(_WIN32)
    if (s < 0 || s >= FD_SETSIZE) return -1;
#endif
    fd_set set;
    FD_ZERO(&set);
    FD_SET(s, &set);
    fd_set errors;
    FD_ZERO(&errors);
    FD_SET(s, &errors);
    timeval timeout{};
    timeout.tv_sec = ms / 1000;
    timeout.tv_usec = (ms % 1000) * 1000;
    const int ready = select(static_cast<int>(s) + 1, for_write ? nullptr : &set, for_write ? &set : nullptr, &errors,
                             &timeout);
    if (ready < 0) return -1;
    return ready == 0 ? 0 : 1;
}

void random_bytes(std::uint8_t* out, std::size_t size) {
    static std::mt19937_64 engine{std::random_device{}()};
    for (std::size_t i = 0; i < size; ++i) out[i] = static_cast<std::uint8_t>(engine());
}

std::string lower(std::string_view text) {
    std::string out(text);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

std::string trim(std::string_view text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && (text[begin] == ' ' || text[begin] == '\t')) ++begin;
    while (end > begin && (text[end - 1u] == ' ' || text[end - 1u] == '\t' || text[end - 1u] == '\r')) --end;
    return std::string(text.substr(begin, end - begin));
}

}  // namespace

std::string sha1(std::string_view data) {
    std::uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
    std::string message(data);
    const std::uint64_t bit_length = static_cast<std::uint64_t>(data.size()) * 8u;
    message += static_cast<char>(0x80);
    while (message.size() % 64u != 56u) message += '\0';
    for (int shift = 56; shift >= 0; shift -= 8) message += static_cast<char>((bit_length >> shift) & 0xFFu);

    for (std::size_t block = 0; block < message.size(); block += 64u) {
        std::uint32_t w[80];
        for (std::size_t i = 0; i < 16u; ++i) {
            const auto* p = reinterpret_cast<const unsigned char*>(message.data() + block + i * 4u);
            w[i] = (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) |
                   (static_cast<std::uint32_t>(p[2]) << 8) | static_cast<std::uint32_t>(p[3]);
        }
        for (std::size_t i = 16; i < 80u; ++i) w[i] = rotl(w[i - 3u] ^ w[i - 8u] ^ w[i - 14u] ^ w[i - 16u], 1);
        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (std::size_t i = 0; i < 80u; ++i) {
            std::uint32_t f;
            std::uint32_t k;
            if (i < 20u) {
                f = (b & c) | (~b & d);
                k = 0x5A827999u;
            } else if (i < 40u) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1u;
            } else if (i < 60u) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDCu;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6u;
            }
            const std::uint32_t temp = rotl(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rotl(b, 30);
            b = a;
            a = temp;
        }
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
    }
    std::string digest;
    for (const std::uint32_t word : h) {
        for (int shift = 24; shift >= 0; shift -= 8) digest += static_cast<char>((word >> shift) & 0xFFu);
    }
    return digest;
}

std::string base64_encode(std::string_view data) {
    static const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    std::size_t i = 0;
    for (; i + 2u < data.size(); i += 3u) {
        const std::uint32_t v = (static_cast<std::uint32_t>(static_cast<unsigned char>(data[i])) << 16) |
                                (static_cast<std::uint32_t>(static_cast<unsigned char>(data[i + 1u])) << 8) |
                                static_cast<std::uint32_t>(static_cast<unsigned char>(data[i + 2u]));
        out += kAlphabet[(v >> 18) & 63u];
        out += kAlphabet[(v >> 12) & 63u];
        out += kAlphabet[(v >> 6) & 63u];
        out += kAlphabet[v & 63u];
    }
    if (i + 1u == data.size()) {
        const std::uint32_t v = static_cast<std::uint32_t>(static_cast<unsigned char>(data[i])) << 16;
        out += kAlphabet[(v >> 18) & 63u];
        out += kAlphabet[(v >> 12) & 63u];
        out += "==";
    } else if (i + 2u == data.size()) {
        const std::uint32_t v = (static_cast<std::uint32_t>(static_cast<unsigned char>(data[i])) << 16) |
                                (static_cast<std::uint32_t>(static_cast<unsigned char>(data[i + 1u])) << 8);
        out += kAlphabet[(v >> 18) & 63u];
        out += kAlphabet[(v >> 12) & 63u];
        out += kAlphabet[(v >> 6) & 63u];
        out += '=';
    }
    return out;
}

std::string websocket_accept_key(std::string_view client_key) {
    std::string joined(client_key);
    joined += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    return base64_encode(sha1(joined));
}

std::optional<WebSocketUrl> parse_websocket_url(std::string_view address) {
    WebSocketUrl url;
    std::string_view rest = address;
    while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) rest.remove_prefix(1);
    while (!rest.empty() && (rest.back() == ' ' || rest.back() == '\t' || rest.back() == '\r')) rest.remove_suffix(1);
    url.port = "80";
    if (rest.substr(0, 6) == "wss://") {
        url.secure = true;
        url.explicit_scheme = true;
        url.port = "443";
        rest.remove_prefix(6);
    } else if (rest.substr(0, 5) == "ws://") {
        url.explicit_scheme = true;
        rest.remove_prefix(5);
    } else if (rest.substr(0, 8) == "https://") {
        url.secure = true;
        url.explicit_scheme = true;
        url.port = "443";
        rest.remove_prefix(8);
    } else if (rest.substr(0, 7) == "http://") {
        url.explicit_scheme = true;
        rest.remove_prefix(7);
    }
    const std::size_t slash = rest.find('/');
    url.path = slash == std::string_view::npos ? "/" : std::string(rest.substr(slash));
    std::string_view authority = rest.substr(0, slash);
    std::string_view host = authority;
    if (!authority.empty() && authority.front() == '[') {
        // An IPv6 literal: [::1]:38281
        const std::size_t close = authority.find(']');
        if (close == std::string_view::npos) return std::nullopt;
        host = authority.substr(1, close - 1u);
        if (close + 1u < authority.size()) {
            if (authority[close + 1u] != ':') return std::nullopt;
            url.port = std::string(authority.substr(close + 2u));
        }
    } else {
        const std::size_t colon = authority.rfind(':');
        if (colon != std::string_view::npos) {
            host = authority.substr(0, colon);
            url.port = std::string(authority.substr(colon + 1u));
        }
    }
    if (host.empty() || url.port.empty()) return std::nullopt;
    for (const char c : url.port) {
        if (c < '0' || c > '9') return std::nullopt;
    }
    url.host = std::string(host);
    return url;
}

std::string encode_frame(std::uint8_t opcode, std::string_view payload, const std::uint8_t* mask) {
    std::string out;
    out += static_cast<char>(0x80u | (opcode & 0x0Fu));
    const std::uint8_t mask_bit = mask != nullptr ? 0x80u : 0u;
    if (payload.size() < 126u) {
        out += static_cast<char>(mask_bit | payload.size());
    } else if (payload.size() <= 0xFFFFu) {
        out += static_cast<char>(mask_bit | 126u);
        out += static_cast<char>((payload.size() >> 8) & 0xFFu);
        out += static_cast<char>(payload.size() & 0xFFu);
    } else {
        out += static_cast<char>(mask_bit | 127u);
        for (int shift = 56; shift >= 0; shift -= 8) {
            out += static_cast<char>((static_cast<std::uint64_t>(payload.size()) >> shift) & 0xFFu);
        }
    }
    if (mask != nullptr) {
        out.append(reinterpret_cast<const char*>(mask), 4u);
        for (std::size_t i = 0; i < payload.size(); ++i) out += static_cast<char>(payload[i] ^ static_cast<char>(mask[i % 4u]));
    } else {
        out.append(payload);
    }
    return out;
}

void FrameDecoder::feed(const char* data, std::size_t size) {
    buffer_.append(data, size);
}

bool FrameDecoder::next(Frame& out) {
    if (error_ || buffer_.size() < 2u) return false;
    const auto* bytes = reinterpret_cast<const unsigned char*>(buffer_.data());
    const bool fin = (bytes[0] & 0x80u) != 0;
    const unsigned reserved = bytes[0] & 0x70u;
    const unsigned opcode = bytes[0] & 0x0Fu;
    const bool masked = (bytes[1] & 0x80u) != 0;
    std::uint64_t length = bytes[1] & 0x7Fu;
    std::size_t offset = 2;
    if (length == 126u) {
        if (buffer_.size() < 4u) return false;
        length = (static_cast<std::uint64_t>(bytes[2]) << 8) | bytes[3];
        offset = 4;
    } else if (length == 127u) {
        if (buffer_.size() < 10u) return false;
        length = 0;
        for (std::size_t i = 0; i < 8u; ++i) length = (length << 8) | bytes[2u + i];
        offset = 10;
    }
    const bool control = (opcode & 0x08u) != 0;
    if (reserved != 0u || length > kMaxPayload || (control && (length > 125u || !fin))) {
        error_ = true;
        return false;
    }
    const std::size_t mask_size = masked ? 4u : 0u;
    if (buffer_.size() < offset + mask_size + length) return false;
    out.fin = fin;
    out.opcode = static_cast<std::uint8_t>(opcode);
    out.payload.assign(buffer_, offset + mask_size, static_cast<std::size_t>(length));
    if (masked) {
        for (std::size_t i = 0; i < out.payload.size(); ++i) out.payload[i] ^= static_cast<char>(bytes[offset + i % 4u]);
    }
    buffer_.erase(0, offset + mask_size + static_cast<std::size_t>(length));
    return true;
}

// ---- TLS (Windows: SChannel) ----

#if defined(_WIN32)
struct TlsSession {
    CredHandle cred{};
    CtxtHandle context{};
    bool have_cred = false;
    bool have_context = false;
    SecPkgContext_StreamSizes sizes{};
    std::string encrypted;  // received from the server, not yet decrypted
    std::string plain;      // decrypted, not yet read
    TlsSession() = default;
    TlsSession(const TlsSession&) = delete;
    TlsSession& operator=(const TlsSession&) = delete;
    ~TlsSession() {
        if (have_context) DeleteSecurityContext(&context);
        if (have_cred) FreeCredentialsHandle(&cred);
    }
};

namespace {

std::string hex_status(long status) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08lX", static_cast<unsigned long>(status));
    return text;
}

bool raw_send_all(Socket s, const char* data, std::size_t size) {
    std::size_t sent = 0;
    while (sent < size) {
        const auto result = ::send(s, data + sent, static_cast<int>(std::min<std::size_t>(size - sent, 1u << 20)), 0);
        if (result <= 0) return false;
        sent += static_cast<std::size_t>(result);
    }
    return true;
}

// The TLS handshake over a connected socket (the server's certificate and name are checked by Windows against its
// trusted roots); false with `error` set when it fails. `remaining_ms` says how long is left.
bool tls_handshake(Socket s, const std::string& host, TlsSession& tls, const std::function<int()>& remaining_ms,
                   std::string& error) {
    SCHANNEL_CRED credential{};
    credential.dwVersion = SCHANNEL_CRED_VERSION;
    credential.dwFlags = SCH_CRED_NO_DEFAULT_CREDS | SCH_USE_STRONG_CRYPTO;
    TimeStamp expiry{};
    SECURITY_STATUS status = AcquireCredentialsHandleA(nullptr, const_cast<SEC_CHAR*>(UNISP_NAME_A), SECPKG_CRED_OUTBOUND,
                                                       nullptr, &credential, nullptr, nullptr, &tls.cred, &expiry);
    if (status != SEC_E_OK) {
        error = "the TLS credentials could not be set up (" + hex_status(status) + ")";
        return false;
    }
    tls.have_cred = true;

    const ULONG request = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY |
                          ISC_RET_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM;
    std::string incoming;
    bool first = true;
    bool need_read = false;
    for (;;) {
        if (need_read) {
            const int ready = wait_socket(s, false, remaining_ms());
            if (ready <= 0) {
                error = ready == 0 ? "the server did not answer the TLS handshake" : "the connection failed";
                return false;
            }
            char buffer[8192];
            const auto count = ::recv(s, buffer, sizeof(buffer), 0);
            if (count <= 0) {
                error = "the server closed the connection during the TLS handshake";
                return false;
            }
            incoming.append(buffer, static_cast<std::size_t>(count));
            need_read = false;
        }

        SecBuffer in_buffers[2];
        in_buffers[0].BufferType = SECBUFFER_TOKEN;
        in_buffers[0].cbBuffer = static_cast<ULONG>(incoming.size());
        in_buffers[0].pvBuffer = incoming.empty() ? nullptr : &incoming[0];
        in_buffers[1].BufferType = SECBUFFER_EMPTY;
        in_buffers[1].cbBuffer = 0;
        in_buffers[1].pvBuffer = nullptr;
        SecBufferDesc in_desc{SECBUFFER_VERSION, 2, in_buffers};
        SecBuffer out_buffer;
        out_buffer.BufferType = SECBUFFER_TOKEN;
        out_buffer.cbBuffer = 0;
        out_buffer.pvBuffer = nullptr;
        SecBufferDesc out_desc{SECBUFFER_VERSION, 1, &out_buffer};
        ULONG attributes = 0;

        status = InitializeSecurityContextA(&tls.cred, first ? nullptr : &tls.context, const_cast<SEC_CHAR*>(host.c_str()),
                                            request, 0, 0, first ? nullptr : &in_desc, 0, &tls.context, &out_desc,
                                            &attributes, &expiry);
        if (first && (status == SEC_I_CONTINUE_NEEDED || status == SEC_E_OK)) tls.have_context = true;
        first = false;

        if (out_buffer.pvBuffer != nullptr) {
            const bool sent = out_buffer.cbBuffer == 0 ||
                              raw_send_all(s, static_cast<const char*>(out_buffer.pvBuffer), out_buffer.cbBuffer);
            FreeContextBuffer(out_buffer.pvBuffer);
            if (!sent) {
                error = "the server closed the connection during the TLS handshake";
                return false;
            }
        }

        // Bytes the call did not use belong to what comes next.
        const auto keep_extra = [&] {
            if (in_buffers[1].BufferType == SECBUFFER_EXTRA && in_buffers[1].cbBuffer > 0 &&
                in_buffers[1].cbBuffer <= incoming.size()) {
                incoming.erase(0, incoming.size() - in_buffers[1].cbBuffer);
            } else {
                incoming.clear();
            }
        };

        if (status == SEC_E_OK) {
            keep_extra();
            tls.encrypted = std::move(incoming);
            break;
        }
        if (status == SEC_I_CONTINUE_NEEDED) {
            keep_extra();
            need_read = incoming.empty();
            continue;
        }
        if (status == SEC_E_INCOMPLETE_MESSAGE) {
            need_read = true;
            continue;
        }
        if (status == static_cast<SECURITY_STATUS>(SEC_E_UNTRUSTED_ROOT) || status == static_cast<SECURITY_STATUS>(CERT_E_UNTRUSTEDROOT)) {
            error = "the server's TLS certificate is not trusted by Windows (" + hex_status(status) + ")";
        } else if (status == static_cast<SECURITY_STATUS>(SEC_E_WRONG_PRINCIPAL)) {
            error = "the server's TLS certificate is for another name (" + hex_status(status) + ")";
        } else {
            error = "the TLS handshake failed (" + hex_status(status) + ")";
        }
        return false;
    }

    status = QueryContextAttributes(&tls.context, SECPKG_ATTR_STREAM_SIZES, &tls.sizes);
    if (status != SEC_E_OK) {
        error = "the TLS session could not be set up (" + hex_status(status) + ")";
        return false;
    }
    return true;
}

bool tls_send(Socket s, TlsSession& tls, const char* data, std::size_t size) {
    while (size > 0) {
        const std::size_t chunk = std::min<std::size_t>(size, tls.sizes.cbMaximumMessage);
        std::string buffer(tls.sizes.cbHeader + chunk + tls.sizes.cbTrailer, '\0');
        std::memcpy(&buffer[tls.sizes.cbHeader], data, chunk);
        SecBuffer parts[4];
        parts[0].BufferType = SECBUFFER_STREAM_HEADER;
        parts[0].cbBuffer = tls.sizes.cbHeader;
        parts[0].pvBuffer = &buffer[0];
        parts[1].BufferType = SECBUFFER_DATA;
        parts[1].cbBuffer = static_cast<ULONG>(chunk);
        parts[1].pvBuffer = &buffer[tls.sizes.cbHeader];
        parts[2].BufferType = SECBUFFER_STREAM_TRAILER;
        parts[2].cbBuffer = tls.sizes.cbTrailer;
        parts[2].pvBuffer = &buffer[tls.sizes.cbHeader + chunk];
        parts[3].BufferType = SECBUFFER_EMPTY;
        parts[3].cbBuffer = 0;
        parts[3].pvBuffer = nullptr;
        SecBufferDesc desc{SECBUFFER_VERSION, 4, parts};
        if (EncryptMessage(&tls.context, 0, &desc, 0) != SEC_E_OK) return false;
        const std::size_t total = static_cast<std::size_t>(parts[0].cbBuffer) + parts[1].cbBuffer + parts[2].cbBuffer;
        if (!raw_send_all(s, buffer.data(), total)) return false;
        data += chunk;
        size -= chunk;
    }
    return true;
}

// Decrypted bytes: the count, 0 when the connection closed, -1 on an error; `timed_out` when nothing came in time.
long tls_read(Socket s, TlsSession& tls, char* out, std::size_t capacity, int timeout_ms, bool& timed_out) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    for (;;) {
        if (!tls.plain.empty()) {
            const std::size_t count = std::min(capacity, tls.plain.size());
            std::memcpy(out, tls.plain.data(), count);
            tls.plain.erase(0, count);
            return static_cast<long>(count);
        }
        if (!tls.encrypted.empty()) {
            SecBuffer parts[4];
            parts[0].BufferType = SECBUFFER_DATA;
            parts[0].cbBuffer = static_cast<ULONG>(tls.encrypted.size());
            parts[0].pvBuffer = &tls.encrypted[0];
            for (int i = 1; i < 4; ++i) {
                parts[i].BufferType = SECBUFFER_EMPTY;
                parts[i].cbBuffer = 0;
                parts[i].pvBuffer = nullptr;
            }
            SecBufferDesc desc{SECBUFFER_VERSION, 4, parts};
            const SECURITY_STATUS status = DecryptMessage(&tls.context, &desc, 0, nullptr);
            if (status == SEC_E_OK) {
                std::string extra;
                for (int i = 1; i < 4; ++i) {
                    if (parts[i].BufferType == SECBUFFER_DATA && parts[i].cbBuffer > 0) {
                        tls.plain.append(static_cast<const char*>(parts[i].pvBuffer), parts[i].cbBuffer);
                    } else if (parts[i].BufferType == SECBUFFER_EXTRA && parts[i].cbBuffer > 0) {
                        extra.assign(static_cast<const char*>(parts[i].pvBuffer), parts[i].cbBuffer);
                    }
                }
                tls.encrypted = std::move(extra);
                continue;
            }
            if (status == SEC_I_CONTEXT_EXPIRED) return 0;
            if (status != SEC_E_INCOMPLETE_MESSAGE) return -1;
        }
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
        const int ready = wait_socket(s, false, static_cast<int>(std::max<long long>(0, left.count())));
        if (ready < 0) return -1;
        if (ready == 0) {
            timed_out = true;
            return 0;
        }
        char buffer[16384];
        const auto count = ::recv(s, buffer, sizeof(buffer), 0);
        if (count <= 0) return 0;
        tls.encrypted.append(buffer, static_cast<std::size_t>(count));
    }
}

}  // namespace
#else
struct TlsSession {};
#endif

WebSocket::WebSocket() = default;

WebSocket::~WebSocket() {
    close();
}

bool WebSocket::is_open() const noexcept {
    return open_;
}

bool WebSocket::tls_available() noexcept {
#if defined(_WIN32)
    return true;
#else
    return false;
#endif
}

bool WebSocket::send_all(const char* data, std::size_t size) {
#if defined(_WIN32)
    if (tls_) return tls_send(to_socket(socket_), *tls_, data, size);
#endif
    return raw_send(data, size);
}

long WebSocket::read_some(char* buffer, std::size_t capacity, int timeout_ms, bool& timed_out) {
    timed_out = false;
    const Socket s = to_socket(socket_);
#if defined(_WIN32)
    if (tls_) return tls_read(s, *tls_, buffer, capacity, timeout_ms, timed_out);
#endif
    const int ready = wait_socket(s, false, timeout_ms);
    if (ready < 0) return -1;
    if (ready == 0) {
        timed_out = true;
        return 0;
    }
    const auto count = ::recv(s, buffer, capacity, 0);
    return count <= 0 ? 0 : static_cast<long>(count);
}

bool WebSocket::raw_send(const char* data, std::size_t size) {
    const Socket s = to_socket(socket_);
    std::size_t sent = 0;
    while (sent < size) {
#if defined(MSG_NOSIGNAL)
        const int flags = MSG_NOSIGNAL;
#else
        const int flags = 0;
#endif
        const auto result = ::send(s, data + sent, static_cast<int>(std::min<std::size_t>(size - sent, 1u << 20)), flags);
        if (result <= 0) return false;
        sent += static_cast<std::size_t>(result);
    }
    return true;
}

bool WebSocket::send_frame(std::uint8_t opcode, std::string_view payload) {
    if (!open_) return false;
    std::uint8_t mask[4];
    random_bytes(mask, sizeof(mask));
    const std::string frame = encode_frame(opcode, payload, mask);
    if (!send_all(frame.data(), frame.size())) {
        close();
        return false;
    }
    return true;
}

bool WebSocket::send_text(std::string_view text) {
    return send_frame(0x1, text);
}

bool WebSocket::connect(const WebSocketUrl& url, int timeout_ms, std::string& error) {
    close();
    if (url.secure && !tls_available()) {
        error = "wss:// (an encrypted connection) is not supported by this build";
        return false;
    }
    ensure_sockets();

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* found = nullptr;
    if (getaddrinfo(url.host.c_str(), url.port.c_str(), &hints, &found) != 0 || found == nullptr) {
        error = "the server's name \"" + url.host + "\" was not found";
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    const auto remaining_ms = [&deadline] {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
        return static_cast<int>(std::max<long long>(0, left.count()));
    };

    Socket connected = kBadSocket;
    for (addrinfo* a = found; a != nullptr && connected == kBadSocket; a = a->ai_next) {
        const Socket s = ::socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (s == kBadSocket) continue;
        if (!set_blocking(s, false)) {
            close_socket(s);
            continue;
        }
        const int result = ::connect(s, a->ai_addr, static_cast<int>(a->ai_addrlen));
        bool ok = result == 0;
        if (!ok && connect_in_progress()) {
            if (wait_socket(s, true, remaining_ms()) == 1) {
                int failure = 0;
#if defined(_WIN32)
                int length = sizeof(failure);
                getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&failure), &length);
#else
                socklen_t length = sizeof(failure);
                getsockopt(s, SOL_SOCKET, SO_ERROR, &failure, &length);
#endif
                ok = failure == 0;
            }
        }
        if (!ok || !set_blocking(s, true)) {
            close_socket(s);
            continue;
        }
        connected = s;
    }
    freeaddrinfo(found);
    if (connected == kBadSocket) {
        error = "could not connect to " + url.host + ":" + url.port;
        return false;
    }

    const int one = 1;
    setsockopt(connected, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&one), sizeof(one));
#if defined(SO_NOSIGPIPE)
    setsockopt(connected, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
    set_send_timeout(connected, 5000);
    socket_ = from_socket(connected);

#if defined(_WIN32)
    if (url.secure) {
        tls_ = std::make_unique<TlsSession>();
        if (!tls_handshake(connected, url.host, *tls_, remaining_ms, error)) {
            close();
            return false;
        }
    }
#endif

    // The upgrade request.
    std::uint8_t key_bytes[16];
    random_bytes(key_bytes, sizeof(key_bytes));
    const std::string key = base64_encode(std::string_view(reinterpret_cast<const char*>(key_bytes), sizeof(key_bytes)));
    std::string request = "GET " + url.path + " HTTP/1.1\r\nHost: " + url.host + ":" + url.port +
                          "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " + key +
                          "\r\nSec-WebSocket-Version: 13\r\n\r\n";
    if (!send_all(request.data(), request.size())) {
        error = "the server closed the connection";
        close();
        return false;
    }

    // Its answer, up to the blank line; anything after it is already the first frames.
    std::string response;
    std::size_t header_end = std::string::npos;
    while (header_end == std::string::npos) {
        if (response.size() > kMaxHandshake) {
            error = "the server's answer is not a WebSocket handshake";
            close();
            return false;
        }
        char buffer[2048];
        bool timed_out = false;
        const long count = read_some(buffer, sizeof(buffer), remaining_ms(), timed_out);
        if (timed_out || count < 0) {
            error = timed_out ? "the server did not answer the WebSocket handshake" : "the connection failed";
            close();
            return false;
        }
        if (count == 0) {
            error = "the server closed the connection during the handshake";
            close();
            return false;
        }
        response.append(buffer, static_cast<std::size_t>(count));
        header_end = response.find("\r\n\r\n");
    }

    const std::string head = response.substr(0, header_end);
    const std::size_t line_end = head.find("\r\n");
    const std::string status = head.substr(0, line_end);
    if (status.find(" 101") == std::string::npos) {
        error = "the server refused the WebSocket upgrade (" + trim(status) + ")";
        close();
        return false;
    }
    const std::string wanted = websocket_accept_key(key);
    bool accepted = false;
    std::size_t pos = line_end == std::string::npos ? head.size() : line_end + 2u;
    while (pos < head.size()) {
        std::size_t eol = head.find("\r\n", pos);
        if (eol == std::string::npos) eol = head.size();
        const std::string line = head.substr(pos, eol - pos);
        const std::size_t colon = line.find(':');
        if (colon != std::string::npos && lower(line.substr(0, colon)) == "sec-websocket-accept") {
            accepted = trim(line.substr(colon + 1u)) == wanted;
        }
        pos = eol + 2u;
    }
    if (!accepted) {
        error = "the server's WebSocket handshake answer is wrong";
        close();
        return false;
    }
    open_ = true;
    const std::string extra = response.substr(header_end + 4u);
    if (!extra.empty()) decoder_.feed(extra.data(), extra.size());
    return true;
}

WebSocket::Event WebSocket::receive(std::string& message, int timeout_ms) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    for (;;) {
        if (!open_) return Event::Closed;
        Frame frame;
        while (decoder_.next(frame)) {
            switch (frame.opcode) {
            case 0x1:  // text
            case 0x2:  // binary: the protocol sends text; a binary message is read the same
                partial_ = std::move(frame.payload);
                if (frame.fin) {
                    message = std::move(partial_);
                    partial_.clear();
                    return Event::Message;
                }
                break;
            case 0x0:  // continuation
                partial_ += frame.payload;
                if (partial_.size() > kMaxPayload) {
                    close();
                    return Event::Closed;
                }
                if (frame.fin) {
                    message = std::move(partial_);
                    partial_.clear();
                    return Event::Message;
                }
                break;
            case 0x9:  // ping
                (void)send_frame(0xA, frame.payload);
                break;
            case 0x8:  // close
                (void)send_frame(0x8, frame.payload.substr(0, 2));
                close();
                return Event::Closed;
            default:  // pong and anything else
                break;
            }
            if (!open_) return Event::Closed;
        }
        if (decoder_.error()) {
            close();
            return Event::Closed;
        }
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
        char buffer[16384];
        bool timed_out = false;
        const long count = read_some(buffer, sizeof(buffer), static_cast<int>(std::max<long long>(0, left.count())), timed_out);
        if (timed_out) return Event::Timeout;
        if (count <= 0) {
            close();
            return Event::Closed;
        }
        decoder_.feed(buffer, static_cast<std::size_t>(count));
    }
}

void WebSocket::close() {
    tls_.reset();
    if (socket_ != -1) {
        close_socket(to_socket(socket_));
        socket_ = -1;
    }
    open_ = false;
    decoder_ = FrameDecoder();
    partial_.clear();
}

}  // namespace galaxy::net
