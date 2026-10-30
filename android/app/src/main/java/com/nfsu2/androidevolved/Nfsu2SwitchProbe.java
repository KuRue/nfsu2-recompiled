package com.nfsu2.androidevolved;

import android.content.Context;
import android.util.Log;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.security.SecureRandom;
import java.util.Base64;

import javax.net.ssl.SNIHostName;
import javax.net.ssl.SSLParameters;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.SSLSocketFactory;

/**
 * Proof of concept for the nfsu2.xyz virtual LAN switch.
 *
 * The native game speaks the original EA LAN protocol over UDP/TCP. The browser
 * port reaches the same switch over a WebSocket at /lan. This probe connects to
 * that endpoint with a minimal TLS + WebSocket client, sends the hello and
 * reports the virtual address and relayed LAN frames. It is deliberately
 * separate from the game's Winsock HLE: once this path is proven, the virtual
 * LAN backend can route the game's sockets through it.
 */
final class Nfsu2SwitchProbe {
    interface Listener { void onStatus(String text); }

    private static final String TAG = "NFSU2Switch";
    private static final String HOST = "nfsu2.xyz";
    private static final int PORT = 443;

    private static volatile Thread worker;
    private static volatile boolean running;

    private Nfsu2SwitchProbe() {}

    static boolean isRunning() { return running; }

    static synchronized void start(Context context, Listener listener) {
        if (running) {
            listener.onStatus("Already connected. Close first.");
            return;
        }
        running = true;
        Context app = context.getApplicationContext();
        worker = new Thread(() -> run(app, listener), "nfsu2-switch-probe");
        worker.start();
    }

    static synchronized void stop() {
        running = false;
        Thread thread = worker;
        worker = null;
        if (thread != null) thread.interrupt();
    }

    private static void run(Context context, Listener listener) {
        listener.onStatus("Connecting to " + HOST + "…");
        SSLSocket socket = null;
        try {
            SSLSocketFactory factory = (SSLSocketFactory) SSLSocketFactory.getDefault();
            socket = (SSLSocket) factory.createSocket();
            socket.connect(new InetSocketAddress(HOST, PORT), 15000);
            socket.setSoTimeout(1000);
            SSLParameters params = socket.getSSLParameters();
            params.setServerNames(java.util.Collections.singletonList(new SNIHostName(HOST)));
            socket.setSSLParameters(params);
            socket.startHandshake();

            String key = websocketKey();
            String request = "GET /lan HTTP/1.1\r\n"
                    + "Host: " + HOST + "\r\n"
                    + "Upgrade: websocket\r\n"
                    + "Connection: Upgrade\r\n"
                    + "Sec-WebSocket-Key: " + key + "\r\n"
                    + "Sec-WebSocket-Version: 13\r\n"
                    + "User-Agent: NFSU2-Android-Probe\r\n"
                    + "\r\n";
            OutputStream rawOut = new BufferedOutputStream(socket.getOutputStream());
            rawOut.write(request.getBytes(StandardCharsets.US_ASCII));
            rawOut.flush();
            InputStream rawIn = new BufferedInputStream(socket.getInputStream());
            String status = readLine(rawIn);
            if (status == null || !status.contains(" 101 ")) {
                listener.onStatus("Upgrade failed: " + status);
                return;
            }
            String line;
            do { line = readLine(rawIn); } while (line != null && !line.isEmpty());
            listener.onStatus("WebSocket open. Sending hello…");

            OutputStream out = rawOut;
            sendText(out, "{\"t\":\"hello\",\"name\":\"NFSU2 Android\",\"deltas\":1,\"lan\":1,"
                    + "\"bundle\":1,\"compact\":1,\"delta\":1,\"fpred\":1}");

            long binaryFrames = 0;
            long lastReport = 0;
            while (running) {
                Frame frame;
                try {
                    frame = readFrame(rawIn);
                } catch (java.net.SocketTimeoutException timeout) {
                    continue;
                }
                if (frame == null) break;
                if (frame.opcode == 8) {
                    listener.onStatus("Server closed the switch connection.");
                    break;
                }
                if (frame.opcode == 9) {
                    sendFrame(out, 10, frame.payload);
                    continue;
                }
                if (frame.opcode == 10) continue;
                if (frame.opcode == 1) {
                    String text = new String(frame.payload, StandardCharsets.UTF_8);
                    Log.i(TAG, "text " + text);
                    if (text.contains("\"t\":\"welcome\"")) {
                        listener.onStatus("Welcome: " + compact(text));
                    } else if (text.contains("\"t\":\"delta\"") || text.contains("\"t\":\"peers\"")) {
                        // Peer list churn is not useful in the PoC dialog.
                    } else {
                        listener.onStatus(compact(text));
                    }
                } else if (frame.opcode == 2) {
                    ++binaryFrames;
                    long now = System.currentTimeMillis();
                    if (binaryFrames == 1) {
                        Log.i(TAG, "first binary frame " + frame.payload.length + " bytes");
                        listener.onStatus("LAN relay active. First frame: " + hex(frame.payload, 32));
                    } else if (now - lastReport > 2000) {
                        lastReport = now;
                        listener.onStatus("LAN relay active. " + binaryFrames + " frames received.");
                    }
                }
            }
        } catch (Exception error) {
            Log.w(TAG, "probe failed", error);
            listener.onStatus("Failed: " + error.getClass().getSimpleName()
                    + (error.getMessage() == null ? "" : ": " + error.getMessage()));
        } finally {
            running = false;
            try { if (socket != null) socket.close(); } catch (IOException ignored) {}
        }
    }

    private static final class Frame {
        final int opcode;
        final byte[] payload;
        Frame(int opcode, byte[] payload) { this.opcode = opcode; this.payload = payload; }
    }

    private static Frame readFrame(InputStream in) throws IOException {
        int b0 = in.read();
        if (b0 < 0) return null;
        int b1 = in.read();
        if (b1 < 0) return null;
        boolean masked = (b1 & 0x80) != 0;
        long length = b1 & 0x7f;
        if (length == 126) length = ((long) in.read() << 8) | in.read();
        else if (length == 127) {
            length = 0;
            for (int i = 0; i < 8; i++) length = (length << 8) | in.read();
        }
        if (length < 0 || length > 4 * 1024 * 1024) throw new IOException("frame too large");
        byte[] mask = null;
        if (masked) {
            mask = new byte[4];
            readFully(in, mask);
        }
        byte[] payload = new byte[(int) length];
        readFully(in, payload);
        if (mask != null) for (int i = 0; i < payload.length; i++) payload[i] ^= mask[i & 3];
        return new Frame(b0 & 0x0f, payload);
    }

    private static void sendText(OutputStream out, String text) throws IOException {
        sendFrame(out, 1, text.getBytes(StandardCharsets.UTF_8));
    }

    private static void sendFrame(OutputStream out, int opcode, byte[] payload) throws IOException {
        out.write(0x80 | (opcode & 0x0f));
        int length = payload.length;
        if (length <= 125) out.write(0x80 | length);
        else if (length <= 0xffff) {
            out.write(0x80 | 126);
            out.write((length >>> 8) & 0xff);
            out.write(length & 0xff);
        } else {
            out.write(0x80 | 127);
            for (int i = 7; i >= 0; i--) out.write((length >>> (i * 8)) & 0xff);
        }
        byte[] mask = new byte[4];
        new SecureRandom().nextBytes(mask);
        out.write(mask);
        for (int i = 0; i < length; i++) out.write(payload[i] ^ mask[i & 3]);
        out.flush();
    }

    private static void readFully(InputStream in, byte[] data) throws IOException {
        int offset = 0;
        while (offset < data.length) {
            int read = in.read(data, offset, data.length - offset);
            if (read < 0) throw new EOFException();
            offset += read;
        }
    }

    private static String readLine(InputStream in) throws IOException {
        StringBuilder line = new StringBuilder();
        int c;
        while ((c = in.read()) >= 0) {
            if (c == '\n') break;
            if (c != '\r') line.append((char) c);
            if (line.length() > 4096) throw new IOException("header line too long");
        }
        return c < 0 && line.length() == 0 ? null : line.toString();
    }

    private static String websocketKey() {
        byte[] nonce = new byte[16];
        new SecureRandom().nextBytes(nonce);
        return Base64.getEncoder().encodeToString(nonce);
    }

    private static String compact(String text) {
        return text.length() <= 400 ? text : text.substring(0, 400) + "…";
    }

    private static String hex(byte[] data, int count) {
        StringBuilder text = new StringBuilder();
        for (int i = 0; i < Math.min(count, data.length); i++) {
            text.append(String.format(java.util.Locale.ROOT, "%02x", data[i] & 0xff));
        }
        return text.toString();
    }
}
