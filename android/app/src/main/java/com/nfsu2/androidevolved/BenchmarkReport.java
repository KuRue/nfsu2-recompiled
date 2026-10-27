package com.nfsu2.androidevolved;

import java.util.HashMap;
import java.util.Locale;

/** Parsed benchmark file. No Android types, so the host check can run it. */
public final class BenchmarkReport {
    public final int count, late, limit, selected, refresh, nextFps;
    public final double p50, p95, p99, maximum, jitter;
    public final boolean steady;
    public final String resolution, action, nextResolution, reason;

    private BenchmarkReport(int count, int late, int limit, int selected, int refresh, int nextFps,
            double p50, double p95, double p99, double maximum, double jitter, boolean steady,
            String resolution, String action, String nextResolution, String reason) {
        this.count = count; this.late = late; this.limit = limit; this.selected = selected;
        this.refresh = refresh; this.nextFps = nextFps; this.p50 = p50; this.p95 = p95;
        this.p99 = p99; this.maximum = maximum; this.jitter = jitter; this.steady = steady;
        this.resolution = resolution; this.action = action; this.nextResolution = nextResolution;
        this.reason = reason;
    }

    public boolean canApply() {
        return action.equals("lower-resolution") || action.equals("lower-frame-rate") || action.equals("raise-resolution");
    }

    public String summary() {
        StringBuilder text = new StringBuilder();
        text.append("Busy frames: ").append(count).append('\n');
        text.append(String.format(Locale.US, "Interval p50 / p95 / p99 / max: %.2f / %.2f / %.2f / %.2f ms\n", p50, p95, p99, maximum));
        text.append(String.format(Locale.US, "Jitter: %.2f ms\n", jitter));
        text.append("Late frames: ").append(late).append(" / ").append(count).append('\n');
        if (selected > 0 && limit > 0 && selected != limit)
            text.append("Selected ").append(selected).append(" FPS, paced at ").append(limit).append(" FPS");
        else if (limit > 0) text.append("Frame cap: ").append(limit).append(" FPS");
        else text.append("Frame cap: uncapped");
        if (refresh > 0) text.append(" on a ").append(refresh).append(" Hz panel");
        text.append(steady ? "\nPace: steady" : "\nPace: exact");
        text.append("\n\n").append(reason);
        if (canApply()) {
            text.append("\n\nApply stores ");
            text.append(nextResolution.equals("720") ? "1280×720" : nextResolution + "%");
            text.append(" and ").append(nextFps == 0 ? "uncapped" : nextFps + " FPS");
            text.append(". The next PLAY uses them.");
        }
        return text.toString();
    }

    public static BenchmarkReport parse(String text) {
        if (text == null) throw new IllegalArgumentException("empty report");
        String[] lines = text.split("\n", -1);
        if (lines.length == 0 || !lines[0].trim().equals("nfs-benchmark 1"))
            throw new IllegalArgumentException("unrecognized report");
        HashMap<String, String> values = new HashMap<>();
        for (int i = 1; i < lines.length; i++) {
            String line = lines[i].trim();
            if (line.isEmpty()) continue;
            int space = line.indexOf(' ');
            if (space <= 0) throw new IllegalArgumentException("bad report line");
            values.put(line.substring(0, space), line.substring(space + 1).trim());
        }
        String action = required(values, "action");
        if (!action.equals("keep") && !action.equals("inconclusive") && !action.equals("lower-resolution")
                && !action.equals("lower-frame-rate") && !action.equals("raise-resolution"))
            throw new IllegalArgumentException("unknown action");
        String resolution = required(values, "resolution");
        String nextResolution = required(values, "next-resolution");
        if (!knownResolution(resolution) || !knownResolution(nextResolution))
            throw new IllegalArgumentException("unknown resolution");
        int nextFps = integer(values, "next-fps");
        if (nextFps != 0 && nextFps != 30 && nextFps != 40 && nextFps != 60 && nextFps != 90 && nextFps != 120)
            throw new IllegalArgumentException("unknown frame cap");
        String pace = required(values, "pace");
        if (!pace.equals("steady") && !pace.equals("exact")) throw new IllegalArgumentException("unknown pace");
        return new BenchmarkReport(integer(values, "count"), integer(values, "late"), integer(values, "limit"),
                integer(values, "selected"), integer(values, "refresh"), nextFps,
                number(values, "p50"), number(values, "p95"), number(values, "p99"), number(values, "max"),
                number(values, "jitter"), pace.equals("steady"), resolution, action, nextResolution,
                required(values, "reason"));
    }

    private static boolean knownResolution(String value) {
        return value.equals("100") || value.equals("85") || value.equals("75") || value.equals("60")
                || value.equals("50") || value.equals("720");
    }

    private static String required(HashMap<String, String> values, String key) {
        String value = values.get(key);
        if (value == null || value.isEmpty()) throw new IllegalArgumentException("missing " + key);
        return value;
    }

    private static int integer(HashMap<String, String> values, String key) {
        try { return Integer.parseInt(required(values, key)); }
        catch (NumberFormatException error) { throw new IllegalArgumentException("bad " + key); }
    }

    private static double number(HashMap<String, String> values, String key) {
        try {
            double value = Double.parseDouble(required(values, key));
            if (!Double.isFinite(value)) throw new IllegalArgumentException("bad " + key);
            return value;
        } catch (NumberFormatException error) { throw new IllegalArgumentException("bad " + key); }
    }
}
