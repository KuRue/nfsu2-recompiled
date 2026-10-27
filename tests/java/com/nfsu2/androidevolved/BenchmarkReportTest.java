package com.nfsu2.androidevolved;
public final class BenchmarkReportTest {
    static void check(boolean value) { if (!value) throw new AssertionError(); }
    public static void main(String[] args) {
        String text = "nfs-benchmark 1\n"
                + "count 120\n"
                + "p50 16.70\n"
                + "p95 17.20\n"
                + "p99 18.00\n"
                + "max 19.50\n"
                + "late 0\n"
                + "jitter 0.40\n"
                + "limit 60\n"
                + "selected 60\n"
                + "refresh 120\n"
                + "pace steady\n"
                + "resolution 75\n"
                + "action raise-resolution\n"
                + "next-resolution 85\n"
                + "next-fps 60\n"
                + "reason The cap held with little jitter. One step up in resolution is the next thing to try.\n";
        BenchmarkReport report = BenchmarkReport.parse(text);
        check(report.count == 120 && report.late == 0 && report.limit == 60 && report.selected == 60);
        check(report.refresh == 120 && report.steady && report.p95 == 17.2 && report.jitter == 0.4);
        check(report.action.equals("raise-resolution") && report.nextResolution.equals("85") && report.nextFps == 60);
        check(report.canApply());
        check(report.summary().contains("Apply stores 85% and 60 FPS"));
        String keep = text.replace("action raise-resolution", "action keep").replace("next-resolution 85", "next-resolution 75");
        check(!BenchmarkReport.parse(keep).canApply());
        check(report.timeline.length == 0 && report.averageFps == 0);
        BenchmarkReport rich = BenchmarkReport.parse(text + "average-fps 116.60\nlow-fps 78.04\n"
                + "timeline 120.0:110.0:121.0:1.20:27000,90.0:60.0:100.0:1.80:29000\n");
        check(rich.averageFps == 116.6 && rich.timeline.length == 10 && rich.timelineMin() == 60 && rich.timelineMax() == 121);
        check(Math.abs(rich.timelineMean(4) - 28000) < 0.01 && rich.summary().contains("Min 60, max 121 FPS"));
        boolean badTimeline = false;
        try { BenchmarkReport.parse(text + "timeline 1:2:3\n"); } catch (IllegalArgumentException error) { badTimeline = true; }
        check(badTimeline);
        BenchmarkReport stuck = BenchmarkReport.parse(text + "stalled 7.5\n");
        check(stuck.stalled == 7.5 && stuck.summary().contains("stuck for 8 s") && report.stalled == 0);
        boolean rejected = false;
        try { BenchmarkReport.parse("not a report\n"); }
        catch (IllegalArgumentException error) { rejected = true; }
        check(rejected);
    }
}
