package com.nfsu2.androidevolved;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.SocketTimeoutException;
import java.nio.charset.StandardCharsets;
import java.security.SecureRandom;
import java.util.Base64;
import java.util.Collections;
import java.util.Locale;
import java.util.concurrent.LinkedBlockingDeque;
import java.util.concurrent.TimeUnit;

import javax.net.ssl.SNIHostName;
import javax.net.ssl.SSLParameters;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.SSLSocketFactory;

/**
 * The nfsu2.xyz /lan WebSocket client, without Android dependencies.
 *
 * The native game's LAN code is routed into a virtual network by the Winsock
 * HLE; this class is the transport under it. It performs the TLS and WebSocket
 * handshakes, sends the switch's hello, keeps the link with pings, reconnects
 * with the token that gets the same virtual address back, and hands binary
 * switch frames to a listener. Outgoing frames are queued from the native
 * side and written by one writer thread, so the game thread never blocks on
 * the network.
 *
 * The hello deliberately does not ask for bundles or compact frames: plain
 * NET_KIND_LAN_* frames are what the native virtual LAN parses.
 */
public final class SwitchSocket {
    public interface Listener {
        /** The switch assigned this browser a virtual address and a resume token. */
        void onWelcome(int vip, int mask, String vipText, String token);
        /** One binary switch frame (NET_KIND_LAN_UDP/TCP/ICMP). */
        void onFrame(byte[] frame);
        /** A human-readable line for the launcher or the log. */
        void onStatus(String text);
        /** The client stopped or the switch refused it for good. */
        void onClosed(String reason);
    }

    private static final String TAG = "NFSU2Switch";
    private final String url;
    private final String name;
    private final Listener listener;
    private final LinkedBlockingDeque<byte[]> outbound = new LinkedBlockingDeque<>(8192);
    private final Object writeLock = new Object();
    private final SecureRandom random = new SecureRandom();

    private volatile boolean running;
    private volatile SSLSocket socket;
    private volatile String token = "";
    private volatile String ticket = "";
    private volatile int vip;
    private volatile String vipText = "";
    private volatile boolean stayOff;
    private Thread reader;
    private Thread writer;
    private int pingId;

    public SwitchSocket(String url, String name, Listener listener) {
        this.url = url;
        this.name = name == null || name.isEmpty() ? "NFSU2 Android" : name;
        this.listener = listener;
    }

    public synchronized void start() {
        if (running) return;
        running = true;
        reader = new Thread(this::run, "nfsu2-switch");
        reader.start();
    }

    public synchronized void stop() {
        running = false;
        closeQuietly(socket);
        socket = null;
        if (reader != null) reader.interrupt();
        if (writer != null) writer.interrupt();
        reader = null;
        writer = null;
        outbound.clear();
    }

    public boolean isOnline() {
        SSLSocket current = socket;
        return running && current != null && current.isConnected() && !current.isClosed();
    }

    /** Queue one binary frame for the switch; drops the oldest when the writer falls behind. */
    public void send(byte[] frame) {
        if (!running || frame == null || frame.length == 0) return;
        if (!outbound.offerLast(frame)) {
            outbound.pollFirst();
            outbound.offerLast(frame);
        }
    }

    private void run() {
        int backoff = 250;
        while (running && !stayOff) {
            boolean welcomed = false;
            try {
                welcomed = connectAndServe();
            } catch (SocketTimeoutException timeout) {
                status("timed out while connecting to nfsu2.xyz");
            } catch (Exception error) {
                status("switch error: " + describe(error));
            } finally {
                closeQuietly(socket);
                socket = null;
                outbound.clear();
                Thread current = writer;
                writer = null;
                if (current != null) current.interrupt();
            }
            if (!running || stayOff) break;
            if (welcomed) backoff = 250;
            try {
                Thread.sleep(backoff);
            } catch (InterruptedException stopped) {
                break;
            }
            backoff = Math.min(backoff * 2, 5000);
        }
        listener.onClosed(stayOff ? "removed from the switch" : "closed");
    }

    /** Connects, serves frames until the link closes, and returns whether a welcome arrived. */
    private boolean connectAndServe() throws Exception {
        java.net.URI uri = java.net.URI.create(url);
        String host = uri.getHost();
        int port = uri.getPort() > 0 ? uri.getPort() : (uri.getScheme().equals("ws") ? 80 : 443);
        if (host == null) throw new IOException("bad /lan URL: " + url);
        if ("ws".equalsIgnoreCase(uri.getScheme())) throw new IOException("the switch requires wss");

        status("connecting to " + host + "…");
        SSLSocket active = (SSLSocket) SSLSocketFactory.getDefault().createSocket();
        active.connect(new InetSocketAddress(host, port), 15000);
        active.setSoTimeout(1000);
        SSLParameters parameters = active.getSSLParameters();
        parameters.setServerNames(Collections.singletonList(new SNIHostName(host)));
        active.setSSLParameters(parameters);
        active.startHandshake();
        socket = active;

        String path = uri.getRawPath() == null || uri.getRawPath().isEmpty() ? "/lan" : uri.getRawPath();
        StringBuilder target = new StringBuilder(path);
        target.append("?queue");
        if ((vip != 0 && !token.isEmpty()) || !ticket.isEmpty()) target.append("&resume");
        String key = websocketKey();
        String request = "GET " + target + " HTTP/1.1\r\n"
                + "Host: " + host + "\r\n"
                + "Upgrade: websocket\r\n"
                + "Connection: Upgrade\r\n"
                + "Sec-WebSocket-Key: " + key + "\r\n"
                + "Sec-WebSocket-Version: 13\r\n"
                + "User-Agent: NFSU2-Android\r\n"
                + "\r\n";
        OutputStream output = new BufferedOutputStream(active.getOutputStream());
        output.write(request.getBytes(StandardCharsets.US_ASCII));
        output.flush();
        InputStream input = new BufferedInputStream(active.getInputStream());
        String statusLine = readLine(input);
        if (statusLine == null || !statusLine.contains(" 101 ")) {
            throw new IOException("upgrade refused: " + statusLine);
        }
        String line;
        do { line = readLine(input); } while (line != null && !line.isEmpty());

        String hello = "{\"t\":\"hello\",\"name\":\"" + escape(name) + "\",\"deltas\":1,\"lan\":1"
                + (vip != 0 && !token.isEmpty()
                    ? ",\"vip\":\"" + escape(vipText) + "\",\"token\":\"" + escape(token) + "\""
                    : "")
                + (ticket.isEmpty() ? "" : ",\"ticket\":\"" + escape(ticket) + "\"")
                + "}";
        sendText(output, hello);
        // A page turns LAN play on again explicitly: the hello already says
        // lan:1, but a queued hello (the switch full) keeps its place through
        // its own messages, and an explicit one costs a single control frame.
        sendText(output, "{\"t\":\"lan\",\"on\":1}");
        status("WebSocket open; waiting for the switch welcome…");

        SSLSocket current = active;
        writer = new Thread(() -> writeLoop(current, output), "nfsu2-switch-tx");
        writer.start();

        boolean welcomed = false;
        boolean wasWelcomed = false;
        while (running && socket == current) {
            Frame frame;
            try {
                frame = readFrame(input);
            } catch (SocketTimeoutException timeout) {
                continue;
            }
            if (frame == null) break;
            if (frame.opcode == 8) {
                int code = frame.payload.length >= 2
                        ? ((frame.payload[0] & 0xff) << 8) | (frame.payload[1] & 0xff) : 1000;
                String reason = frame.payload.length > 2
                        ? new String(frame.payload, 2, frame.payload.length - 2, StandardCharsets.UTF_8) : "";
                if (code == 4001 || code == 4003) {
                    stayOff = true;
                    listener.onStatus(code == 4001
                            ? "The operator removed this player from the switch."
                            : "This player is banned from the switch.");
                } else if (code != 1000) {
                    status("switch closed (" + code + (reason.isEmpty() ? "" : " " + reason) + ")");
                }
                break;
            }
            if (frame.opcode == 9) {
                sendFrame(output, 10, frame.payload);
                continue;
            }
            if (frame.opcode == 10) continue;
            if (frame.opcode == 1) {
                String text = new String(frame.payload, StandardCharsets.UTF_8);
                String type = stringField(text, "t");
                if ("welcome".equals(type)) {
                    ticket = "";
                    long address = numberField(text, "vip");
                    if (address < 0) continue;
                    vip = (int) address;
                    vipText = ipText(vip);
                    token = stringField(text, "token");
                    String maskText = stringField(text, "mask");
                    int mask = parseIp(maskText, 0xffffff00);
                    welcomed = true;
                    wasWelcomed = true;
                    listener.onWelcome(vip, mask, vipText, token);
                } else if ("queue".equals(type)) {
                    String nextTicket = stringField(text, "ticket");
                    if (!nextTicket.isEmpty()) ticket = nextTicket;
                    long position = numberField(text, "pos");
                    long total = numberField(text, "of");
                    status(position > 0 && total > 0
                            ? "The switch is full: in line, " + position + " of " + total
                            : "The switch is full: waiting for a place");
                } else if ("kick".equals(type) || "ban".equals(type)) {
                    String message = stringField(text, "msg");
                    listener.onStatus(("ban".equals(type) ? "Banned" : "Kicked")
                            + (message.isEmpty() ? "" : ": " + message));
                }
                continue;
            }
            if (frame.opcode == 2 || frame.opcode == 0) {
                listener.onFrame(frame.payload);
            }
        }
        return wasWelcomed;
    }

    private void writeLoop(SSLSocket current, OutputStream output) {
        while (running && socket == current) {
            try {
                byte[] frame = outbound.poll(2, TimeUnit.SECONDS);
                if (frame != null) {
                    sendFrame(output, 2, frame);
                } else if (isOnline()) {
                    int id = ++pingId;
                    sendText(output, "{\"t\":\"ping\",\"id\":" + id + "}");
                }
            } catch (InterruptedException stopped) {
                break;
            } catch (IOException error) {
                if (running) status("switch write failed: " + describe(error));
                break;
            }
        }
    }

    // ---- WebSocket framing -----------------------------------------------------
    private static final class Frame {
        final int opcode;
        final byte[] payload;
        Frame(int opcode, byte[] payload) { this.opcode = opcode; this.payload = payload; }
    }

    private static Frame readFrame(InputStream input) throws IOException {
        int first = input.read();
        if (first < 0) return null;
        int second = input.read();
        if (second < 0) return null;
        boolean masked = (second & 0x80) != 0;
        long length = second & 0x7f;
        if (length == 126) length = ((long) readByte(input) << 8) | readByte(input);
        else if (length == 127) {
            length = 0;
            for (int i = 0; i < 8; i++) length = (length << 8) | readByte(input);
        }
        if (length < 0 || length > 16 * 1024 * 1024) throw new IOException("frame too large");
        byte[] mask = null;
        if (masked) {
            mask = new byte[4];
            readFully(input, mask);
        }
        byte[] payload = new byte[(int) length];
        readFully(input, payload);
        if (mask != null) for (int i = 0; i < payload.length; i++) payload[i] ^= mask[i & 3];
        int opcode = first & 0x0f;
        return new Frame(opcode, payload);
    }

    private void sendText(OutputStream output, String text) throws IOException {
        sendFrame(output, 1, text.getBytes(StandardCharsets.UTF_8));
    }

    private void sendFrame(OutputStream output, int opcode, byte[] payload) throws IOException {
        synchronized (writeLock) {
            output.write(0x80 | (opcode & 0x0f));
            int length = payload.length;
            if (length <= 125) output.write(0x80 | length);
            else if (length <= 0xffff) {
                output.write(0x80 | 126);
                output.write((length >>> 8) & 0xff);
                output.write(length & 0xff);
            } else {
                output.write(0x80 | 127);
                for (int i = 7; i >= 0; i--) output.write((length >>> (i * 8)) & 0xff);
            }
            byte[] mask = new byte[4];
            random.nextBytes(mask);
            output.write(mask);
            for (int i = 0; i < length; i++) output.write(payload[i] ^ mask[i & 3]);
            output.flush();
        }
    }

    private static void readFully(InputStream input, byte[] data) throws IOException {
        int offset = 0;
        while (offset < data.length) {
            int read = input.read(data, offset, data.length - offset);
            if (read < 0) throw new EOFException();
            offset += read;
        }
    }

    private static int readByte(InputStream input) throws IOException {
        int value = input.read();
        if (value < 0) throw new EOFException();
        return value;
    }

    private static String readLine(InputStream input) throws IOException {
        StringBuilder line = new StringBuilder();
        int c;
        while ((c = input.read()) >= 0) {
            if (c == '\n') break;
            if (c != '\r') line.append((char) c);
            if (line.length() > 8192) throw new IOException("header line too long");
        }
        return c < 0 && line.length() == 0 ? null : line.toString();
    }

    private String websocketKey() {
        byte[] nonce = new byte[16];
        random.nextBytes(nonce);
        return Base64.getEncoder().encodeToString(nonce);
    }

    private void status(String text) {
        listener.onStatus(text);
    }

    private static void closeQuietly(SSLSocket socket) {
        if (socket == null) return;
        try { socket.close(); } catch (IOException ignored) {}
    }

    private static String describe(Throwable error) {
        String message = error.getMessage();
        return error.getClass().getSimpleName() + (message == null || message.isEmpty() ? "" : ": " + message);
    }

    private static String escape(String value) {
        StringBuilder out = new StringBuilder(value.length() + 8);
        for (int i = 0; i < value.length(); i++) {
            char c = value.charAt(i);
            if (c == '"' || c == '\\') out.append('\\').append(c);
            else if (c < 0x20) out.append(String.format(Locale.ROOT, "\\u%04x", (int) c));
            else out.append(c);
        }
        return out.toString();
    }

    // ---- the few JSON fields the switch sends ---------------------------------
    static String stringField(String json, String key) {
        int at = fieldAt(json, key);
        if (at < 0 || at >= json.length() || json.charAt(at) != '"') return "";
        StringBuilder value = new StringBuilder();
        for (int i = at + 1; i < json.length(); i++) {
            char c = json.charAt(i);
            if (c == '"') break;
            if (c == '\\' && i + 1 < json.length()) {
                char next = json.charAt(++i);
                if (next == 'n') value.append('\n');
                else if (next == 'r') value.append('\r');
                else if (next == 't') value.append('\t');
                else if (next == 'u' && i + 4 < json.length()) {
                    try {
                        value.append((char) Integer.parseInt(json.substring(i + 1, i + 5), 16));
                        i += 4;
                    } catch (NumberFormatException ignored) {}
                } else value.append(next);
            } else value.append(c);
        }
        return value.toString();
    }

    static long numberField(String json, String key) {
        int at = fieldAt(json, key);
        if (at < 0) return -1;
        int end = at;
        while (end < json.length() && "-0123456789".indexOf(json.charAt(end)) >= 0) end++;
        if (end == at) return -1;
        try { return Long.parseLong(json.substring(at, end)); }
        catch (NumberFormatException error) { return -1; }
    }

    private static int fieldAt(String json, String key) {
        String needle = "\"" + key + "\"";
        int from = 0;
        while (from < json.length()) {
            int at = json.indexOf(needle, from);
            if (at < 0) return -1;
            int colon = at + needle.length();
            while (colon < json.length() && Character.isWhitespace(json.charAt(colon))) colon++;
            if (colon < json.length() && json.charAt(colon) == ':') {
                colon++;
                while (colon < json.length() && Character.isWhitespace(json.charAt(colon))) colon++;
                return colon;
            }
            from = at + needle.length();
        }
        return -1;
    }

    static int parseIp(String text, int fallback) {
        if (text == null) return fallback;
        String[] parts = text.trim().split("\\.");
        if (parts.length != 4) return fallback;
        int value = 0;
        try {
            for (String part : parts) {
                int octet = Integer.parseInt(part);
                if (octet < 0 || octet > 255) return fallback;
                value = (value << 8) | octet;
            }
        } catch (NumberFormatException error) {
            return fallback;
        }
        return value;
    }

    static String ipText(int value) {
        return ((value >>> 24) & 255) + "." + ((value >>> 16) & 255) + "."
                + ((value >>> 8) & 255) + "." + (value & 255);
    }
}
