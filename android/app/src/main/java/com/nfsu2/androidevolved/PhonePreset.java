package com.nfsu2.androidevolved;

/** Explicit starting points, not promises of achieved FPS or automatic thermal tuning. */
enum PhonePreset {
    BATTERY("Batería · 50% / 30 FPS", "50", "30", "2"),
    BALANCED("Equilibrado · 75% / 60 FPS", "75", "60", "2"),
    QUALITY("Calidad · nativa / 60 FPS", "100", "60", "2");

    final String label, resolution, fps, backBuffers;

    PhonePreset(String label, String resolution, String fps, String backBuffers) {
        this.label = label;
        this.resolution = resolution;
        this.fps = fps;
        this.backBuffers = backBuffers;
    }

    boolean matches(String resolution, String fps, String backBuffers) {
        return this.resolution.equals(resolution) && this.fps.equals(fps)
                && this.backBuffers.equals(backBuffers);
    }
}
