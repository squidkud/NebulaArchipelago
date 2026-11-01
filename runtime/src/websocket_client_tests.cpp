// The WebSocket client's pieces (galaxy/websocket_client.h): the handshake's hash, addresses and frames.
#include "galaxy/websocket_client.h"

#include <cstdint>
#include <cstdio>
#include <string>

namespace {
int s_failures = 0;
void expect(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++s_failures;
    }
}

std::string hex(const std::string& bytes) {
    static const char digits[] = "0123456789abcdef";
    std::string out;
    for (const char c : bytes) {
        out += digits[(static_cast<unsigned char>(c) >> 4) & 15u];
        out += digits[static_cast<unsigned char>(c) & 15u];
    }
    return out;
}
}  // namespace

int main() {
    namespace net = galaxy::net;

    expect(hex(net::sha1("abc")) == "a9993e364706816aba3e25717850c26c9cd0d89d", "SHA-1 of abc");
    expect(hex(net::sha1("")) == "da39a3ee5e6b4b0d3255bfef95601890afd80709", "SHA-1 of nothing");
    expect(hex(net::sha1("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")) == "84983e441c3bd26ebaae4aa1f95129e5e54670f1",
           "SHA-1 over two blocks");
    expect(net::base64_encode("") == "" && net::base64_encode("f") == "Zg==" && net::base64_encode("fo") == "Zm8=" &&
               net::base64_encode("foo") == "Zm9v" && net::base64_encode("foobar") == "Zm9vYmFy",
           "base64 (RFC 4648's examples)");
    expect(net::websocket_accept_key("dGhlIHNhbXBsZSBub25jZQ==") == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=",
           "the accept key of RFC 6455's example");

    {
        const auto url = net::parse_websocket_url("archipelago.gg:38281");
        expect(url && url->host == "archipelago.gg" && url->port == "38281" && url->path == "/" && !url->secure, "host:port");
    }
    {
        const auto url = net::parse_websocket_url("  ws://localhost:1234/room  ");
        expect(url && url->host == "localhost" && url->port == "1234" && url->path == "/room", "ws:// with a path");
    }
    {
        const auto url = net::parse_websocket_url("wss://example.org");
        expect(url && url->secure && url->port == "443", "wss:// is secure");
    }
    {
        const auto url = net::parse_websocket_url("[::1]:5000");
        expect(url && url->host == "::1" && url->port == "5000", "an IPv6 address");
    }
    expect(!net::parse_websocket_url("").has_value(), "no address");
    expect(!net::parse_websocket_url("host:abc").has_value(), "a port that is not a number");
    expect(!net::parse_websocket_url(":38281").has_value(), "no host");

    {
        // RFC 6455, 5.7: the masked "Hello".
        const std::uint8_t mask[4] = {0x37, 0xfa, 0x21, 0x3d};
        const std::string frame = net::encode_frame(0x1, "Hello", mask);
        expect(hex(frame) == "818537fa213d7f9f4d5158", "a masked text frame");
        net::FrameDecoder decoder;
        decoder.feed(frame.data(), 3);
        net::Frame out;
        expect(!decoder.next(out), "a frame is not complete until its last byte");
        decoder.feed(frame.data() + 3, frame.size() - 3u);
        expect(decoder.next(out) && out.fin && out.opcode == 1 && out.payload == "Hello", "the frame is read, unmasked");
        expect(!decoder.next(out), "nothing is left");
    }
    {
        // The server's frames are not masked; a long payload uses the 16-bit and 64-bit lengths.
        net::FrameDecoder decoder;
        const std::string medium(300, 'm');
        const std::string large(70000, 'L');
        const std::string stream = net::encode_frame(0x1, "a", nullptr) + net::encode_frame(0x1, medium, nullptr) +
                                   net::encode_frame(0x2, large, nullptr);
        for (const char c : stream) decoder.feed(&c, 1);  // one byte at a time
        net::Frame out;
        expect(decoder.next(out) && out.payload == "a", "a short frame");
        expect(decoder.next(out) && out.payload == medium, "a 16-bit length");
        expect(decoder.next(out) && out.opcode == 2 && out.payload == large, "a 64-bit length");
        expect(!decoder.error(), "no error");
    }
    {
        // A fragmented message: text without FIN, then a continuation with it.
        net::FrameDecoder decoder;
        std::string first = net::encode_frame(0x1, "He", nullptr);
        first[0] = static_cast<char>(first[0] & 0x7F);  // clear FIN
        const std::string rest = net::encode_frame(0x0, "llo", nullptr);
        decoder.feed(first.data(), first.size());
        decoder.feed(rest.data(), rest.size());
        net::Frame out;
        expect(decoder.next(out) && !out.fin && out.opcode == 1 && out.payload == "He", "the first fragment");
        expect(decoder.next(out) && out.fin && out.opcode == 0 && out.payload == "llo", "the last fragment");
    }
    {
        net::FrameDecoder decoder;
        const std::string bad = std::string("\xC1\x00", 2);  // a reserved bit set
        decoder.feed(bad.data(), bad.size());
        net::Frame out;
        expect(!decoder.next(out) && decoder.error(), "a reserved bit is an error");
    }
    {
        net::FrameDecoder decoder;
        std::string ping = net::encode_frame(0x9, std::string(200, 'p'), nullptr);  // control payloads hold 125 at most
        decoder.feed(ping.data(), ping.size());
        net::Frame out;
        expect(!decoder.next(out) && decoder.error(), "an oversized control frame is an error");
    }

    // A connection that cannot be made says why.
    {
        net::WebSocket socket;
        std::string error;
        net::WebSocketUrl secure;
        secure.host = "localhost";
        secure.port = "443";
        secure.path = "/";
        secure.secure = true;
        expect(!socket.connect(secure, 500, error) && !error.empty(), "wss:// is refused with a reason");
        net::WebSocketUrl closed;
        closed.host = "127.0.0.1";
        closed.port = "1";  // nothing listens there
        closed.path = "/";
        error.clear();
        expect(!socket.connect(closed, 1000, error) && !error.empty(), "a closed port is reported");
    }

    {
        net::WebSocket idle;  // never opened: no ping can be sent, and the idle clock reads a small, non-negative time
        expect(!idle.send_ping() && idle.idle_ms() >= 0 && idle.idle_ms() < 60000, "a closed socket cannot ping");
    }

    if (s_failures == 0) std::printf("websocket_client: all tests passed\n");
    return s_failures == 0 ? 0 : 1;
}
