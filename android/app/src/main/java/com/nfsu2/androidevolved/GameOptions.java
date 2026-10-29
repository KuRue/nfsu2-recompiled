package com.nfsu2.androidevolved;

import android.content.Context;
import android.content.SharedPreferences;

/**
 * Launcher options. GameActivity reads them when the game process starts and hands them to the native
 * runtime before the guest boots; changing one takes effect on the next PLAY.
 */
final class GameOptions {
    static final class Option {
        final String key, title, defaultValue;
        final String[] values, labels;
        Option(String key, String title, String defaultValue, String[] values, String[] labels) {
            this.key = key; this.title = title; this.defaultValue = defaultValue; this.values = values; this.labels = labels;
        }
        String label(String value) {
            for (int i = 0; i < values.length; i++) if (values[i].equals(value)) return labels[i];
            return value;
        }
    }

    static final String RESOLUTION = "resolution", FPS = "fps", FRAME_PACE = "frame_pace", WIDESCREEN = "widescreen", LATENCY = "latency",
            FEEDBACK_SOURCE = "phone_feedback_source", FEEDBACK = "phone_feedback", PEDALS = "controller_pedals", TEXTURES = "textures", HD_PACKS = "hd_packs", FILTERING = "filtering", SHARPEN = "sharpen", BLOOM = "bloom", LOOK = "lighting_look", EFFECTS = "night_effects", ATMOSPHERE = "atmosphere", TREATMENT = "treatment", DETAIL = "detail", REFLECTIONS = "reflection_quality", MINIMAP = "minimap", INTRO = "intro", LANGUAGE = "language";

    static final Option[] ALL = {
            new Option(FEEDBACK_SOURCE, "Phone feedback source", "physics",
                    new String[] {"physics", "legacy"}, new String[] {"Vehicle physics · slip estimates", "Game force effects · previous mapping"}),
            new Option(FEEDBACK, "Phone vibration", "50",
                    new String[] {"0", "25", "50", "100"}, new String[] {"Off", "Light", "Medium", "Strong"}),
            new Option(PEDALS, "Controller pedals", "triggers",
                    new String[] {"triggers", "game"}, new String[] {"RT gas / LT brake · compatibility", "Game bindings · manual axis setup"}),
            new Option(HD_PACKS, "Texture packs", "off",
                    new String[] {"off", "on"}, new String[] {"Original textures", "HD HUD + world pack"}),
            new Option(FILTERING, "Texture filtering", "0",
                    new String[] {"0", "4", "8", "16"}, new String[] {"Game default", "4× anisotropic", "8× anisotropic", "16× anisotropic"}),
            new Option(SHARPEN, "Sharpening", "0",
                    new String[] {"0", "15", "30"}, new String[] {"Off", "Subtle", "Crisp"}),
            new Option(BLOOM, "Bloom and glow", "0",
                    new String[] {"0", "1", "2"}, new String[] {"Off", "Subtle", "Strong"}),
            new Option(LOOK, "Lighting look", "0",
                    new String[] {"0", "1", "2", "3", "4"},
                    new String[] {"Original", "Night lighting + balanced color", "Neon night · cool shadows",
                            "Filmic night · soft highlights", "Midnight · deepest blacks"}),
            new Option(TREATMENT, "Visual treatment", "0",
                    new String[] {"0", "1", "2"},
                    new String[] {"Original", "Bleach bypass", "Bleach + depth of field"}),
            new Option(DETAIL, "Detail level", "0",
                    new String[] {"0", "1", "2"},
                    new String[] {"Original (game setting)", "High", "Maximum"}),
            new Option(ATMOSPHERE, "Atmosphere", "0",
                    new String[] {"0", "1", "2"},
                    new String[] {"Original", "Fog and horizon haze", "Fog + rain"}),
            new Option(EFFECTS, "Night effects", "0",
                    new String[] {"0", "1", "2"},
                    new String[] {"Original", "Glow, light streaks, tinting, particles",
                            "…plus motion blur"}),
            new Option(REFLECTIONS, "Reflections", "0",
                    new String[] {"0", "1"}, new String[] {"Original resolution", "On · 2× mirror and car resolution"}),
            new Option(RESOLUTION, "Internal resolution", "75",
                    new String[] {"100", "85", "75", "60", "50", "720"},
                    new String[] {"Native · highest quality", "85%", "75% · balanced", "60%", "50% · best performance",
                            "1280×720 · 16:9"}),
            new Option(FPS, "Frame rate limit", "120",
                    new String[] {"30", "40", "60", "90", "120", "0"},
                    new String[] {"30 FPS · save battery", "40 FPS · even on 120 Hz", "60 FPS",
                            "90 FPS · steady pace uses 60 on 120 Hz", "120 FPS", "Uncapped"}),
            new Option(FRAME_PACE, "Frame pace", "steady",
                    new String[] {"steady", "exact"},
                    new String[] {"Steady · even slices of the display", "Exact · selected cap, may jitter"}),
            new Option(WIDESCREEN, "Widescreen", "on",
                    new String[] {"on", "off"},
                    new String[] {"Corrected HUD and camera", "Original 4:3, stretched"}),
            new Option(MINIMAP, "Minimap", "top",
                    new String[] {"top", "bottom"},
                    new String[] {"Top left", "Bottom left (original)"}),
            new Option(INTRO, "Intro movies", "skip",
                    new String[] {"skip", "play"},
                    new String[] {"Skip · straight to the menu", "Play · EA, THX and opening movie"}),
            new Option(LATENCY, "CPU/GPU synchronization", "2",
                    new String[] {"2", "1"},
                    new String[] {"Performance · CPU and GPU in parallel", "Low latency · lower FPS"}),
            new Option(TEXTURES, "Compressed textures", "auto",
                    new String[] {"auto", "cpu"},
                    new String[] {"Automatic · GPU when supported", "Convert on CPU · compatibility"}),
    };

    private static final String PREFS = "nfsu2_game";

    private GameOptions() {}

    static SharedPreferences prefs(Context context) {
        return context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    static Option find(String key) {
        for (Option o : ALL) if (o.key.equals(key)) return o;
        throw new IllegalArgumentException(key);
    }

    static String get(Context context, String key) {
        Option o = find(key);
        String value = prefs(context).getString(key, o.defaultValue);
        for (String allowed : o.values) if (allowed.equals(value)) return value;
        return o.defaultValue;
    }

    static void set(Context context, String key, String value) {
        prefs(context).edit().putString(key, value).apply();
    }

    static PhonePreset phonePreset(Context context) {
        for (PhonePreset preset : PhonePreset.values())
            if (preset.matches(get(context, RESOLUTION), get(context, FPS), get(context, LATENCY)))
                return preset;
        return null; // Individual settings remain editable and are shown as custom.
    }

    static void applyPhonePreset(Context context, PhonePreset preset) {
        // One transaction; leave language, driver, controls, textures and saves alone.
        prefs(context).edit().putString(RESOLUTION, preset.resolution)
                .putString(FPS, preset.fps).putString(LATENCY, preset.backBuffers).apply();
    }

    static final String ONLINE_SERVER = "online_server", DEFAULT_ONLINE_SERVER = "eahub.eu";

    /** EA Nation replacement server: the game's *.ea.com hosts resolve to this name or IPv4 address. */
    static String onlineServer(Context context) {
        String value = prefs(context).getString(ONLINE_SERVER, DEFAULT_ONLINE_SERVER);
        return validServer(value) ? value : DEFAULT_ONLINE_SERVER;
    }

    static boolean validServer(String value) {
        return value != null && value.length() <= 253 && value.matches("[A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?(\\.[A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?)*");
    }

    /** The game's language folder name (LANGUAGES/<file>.bin) chosen in the launcher. */
    static String language(Context context) {
        return prefs(context).getString(LANGUAGE, "English UK");
    }

    /** Render size for the selected resolution on a display of width × height (landscape). */
    static int[] renderSize(Context context, int width, int height) {
        String value = get(context, RESOLUTION);
        if (value.equals("720")) return new int[] {1280, 720};
        float scale = Integer.parseInt(value) / 100f;
        return new int[] {Math.round(width * scale / 2f) * 2, Math.round(height * scale / 2f) * 2};
    }
}
