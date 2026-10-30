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
            FEEDBACK_SOURCE = "phone_feedback_source", FEEDBACK = "phone_feedback", PEDALS = "controller_pedals", TEXTURES = "textures", HD_PACKS = "hd_packs", FILTERING = "filtering", SHARPEN = "sharpen", BLOOM = "bloom", LOOK = "lighting_look", REFLECTIONS = "reflection_quality", MINIMAP = "minimap", INTRO = "intro", LANGUAGE = "language",
            WORLD_DETAIL = "world_detail", CAR_DETAIL = "car_detail",
            CAR_SHADOW = "car_shadow", CAR_HEADLIGHT = "car_headlight", CAR_ENV_DETAIL = "car_env_detail",
            CAR_ENV_UPDATE = "car_env_update", ROAD_REFLECT = "road_reflect_detail", CROWD = "crowd",
            LIGHT_TRAILS = "light_trails", LIGHT_GLOW = "light_glow", PARTICLES = "particles",
            MOTION_BLUR = "motion_blur", FOG = "fog", DOF = "dof", OVERBRIGHT = "overbright",
            BLEACH = "bleach", RAIN = "rain";

    /** Launcher row -> the game registry key it overrides (game = use the saved setting). */
    static final String[][] DISPLAY_MAP = {
            {WORLD_DETAIL, "g_worldlodlevel"}, {CAR_DETAIL, "g_carlodlevel"},
            {CAR_SHADOW, "g_carshadowenable"}, {CAR_HEADLIGHT, "g_carheadlightenable"},
            {CAR_ENV_DETAIL, "g_carenvironmentmapenable"}, {CAR_ENV_UPDATE, "g_carenvironmentmapupdatedata"},
            {ROAD_REFLECT, "g_roadreflectionenable"}, {CROWD, "g_crowdenable"},
            {LIGHT_TRAILS, "g_lightstreaksenable"}, {LIGHT_GLOW, "g_lightglowenable"},
            {PARTICLES, "g_particlesystemenable"}, {MOTION_BLUR, "g_motionblurenable"},
            {FOG, "g_fogenable"}, {DOF, "g_depthoffieldenable"},
            {OVERBRIGHT, "g_overbrightenable"}, {BLEACH, "g_bleachbypassenable"},
            {RAIN, "g_rainenable"},
    };

    static boolean isDisplayOption(Option option) {
        for (String[] map : DISPLAY_MAP) if (map[0].equals(option.key)) return true;
        return false;
    }

    static final Option[] ALL = {
            new Option(FEEDBACK_SOURCE, "Feedback source", "physics",
                    new String[] {"physics", "legacy"}, new String[] {"Physics", "Legacy"}),
            new Option(FEEDBACK, "Phone vibration", "50",
                    new String[] {"0", "25", "50", "100"}, new String[] {"Off", "Light", "Medium", "Strong"}),
            new Option(PEDALS, "Controller pedals", "triggers",
                    new String[] {"triggers", "game"}, new String[] {"Triggers", "Game bindings"}),
            new Option(HD_PACKS, "HD texture packs", "off",
                    new String[] {"off", "on"}, new String[] {"Original", "HD"}),
            new Option(FILTERING, "Texture filtering", "0",
                    new String[] {"0", "4", "8", "16"}, new String[] {"Game default", "4× anisotropic", "8× anisotropic", "16× anisotropic"}),
            new Option(SHARPEN, "Sharpening", "0",
                    new String[] {"0", "15", "30"}, new String[] {"Off", "Subtle", "Crisp"}),
            new Option(BLOOM, "Bloom", "0",
                    new String[] {"0", "1", "2"}, new String[] {"Off", "Subtle", "Strong"}),
            new Option(LOOK, "Color grade", "0",
                    new String[] {"0", "1", "2", "3", "4"},
                    new String[] {"Original", "Night", "Neon night", "Filmic", "Midnight"}),
            new Option(WORLD_DETAIL, "World LOD", "2",
                    new String[] {"0", "1", "2"}, new String[] {"Low", "Medium", "High"}),
            new Option(CAR_DETAIL, "Car LOD", "1",
                    new String[] {"0", "1", "2"}, new String[] {"Low", "Medium", "High"}),
            new Option(CAR_SHADOW, "Car shadow", "2",
                    new String[] {"0", "1", "2"}, new String[] {"Off", "Soft", "Neon"}),
            new Option(CAR_HEADLIGHT, "Headlights", "1",
                    new String[] {"0", "1"}, new String[] {"Off", "On"}),
            new Option(CAR_ENV_DETAIL, "Car reflection detail", "1",
                    new String[] {"0", "1", "2"}, new String[] {"Off", "Low", "High"}),
            new Option(CAR_ENV_UPDATE, "Car reflection updates", "1",
                    new String[] {"0", "1"}, new String[] {"Off", "On"}),
            new Option(ROAD_REFLECT, "Road reflection detail", "2",
                    new String[] {"0", "1", "2", "3"},
                    new String[] {"Off", "Low", "Medium", "High"}),
            new Option(CROWD, "Crowds", "1",
                    new String[] {"0", "1"}, new String[] {"Off", "On"}),
            new Option(LIGHT_TRAILS, "Light trails", "1",
                    new String[] {"0", "1"}, new String[] {"Off", "On"}),
            new Option(LIGHT_GLOW, "Light glow", "1",
                    new String[] {"0", "1"}, new String[] {"Off", "On"}),
            new Option(PARTICLES, "Particles", "1",
                    new String[] {"0", "1"}, new String[] {"Off", "On"}),
            new Option(MOTION_BLUR, "Motion blur", "1",
                    new String[] {"0", "1"}, new String[] {"Off", "On"}),
            new Option(FOG, "Fog", "1",
                    new String[] {"0", "1"}, new String[] {"Off", "On"}),
            new Option(DOF, "Depth blur", "1",
                    new String[] {"0", "1"}, new String[] {"Off", "On"}),
            new Option(OVERBRIGHT, "Overbright", "1",
                    new String[] {"0", "1"}, new String[] {"Off", "On"}),
            new Option(BLEACH, "Bleach bypass", "0",
                    new String[] {"0", "1"}, new String[] {"Off", "On"}),
            new Option(RAIN, "Rain", "0",
                    new String[] {"0", "1"}, new String[] {"Off", "On"}),
            new Option(REFLECTIONS, "Reflection resolution (2×)", "0",
                    new String[] {"0", "1"}, new String[] {"Original", "2×"}),
            new Option(RESOLUTION, "Internal resolution", "75",
                    new String[] {"100", "85", "75", "60", "50", "720"},
                    new String[] {"Native", "85%", "75%", "60%", "50%",
                            "720p"}),
            new Option(FPS, "FPS limit", "120",
                    new String[] {"30", "40", "60", "90", "120", "0"},
                    new String[] {"30 FPS", "40 FPS", "60 FPS",
                            "90 FPS", "120 FPS", "Uncapped"}),
            new Option(FRAME_PACE, "Frame pace", "steady",
                    new String[] {"steady", "exact"},
                    new String[] {"Steady", "Exact"}),
            new Option(WIDESCREEN, "Widescreen", "on",
                    new String[] {"on", "off"},
                    new String[] {"Corrected", "Stretched"}),
            new Option(MINIMAP, "Minimap", "top",
                    new String[] {"top", "bottom"},
                    new String[] {"Top left", "Bottom left"}),
            new Option(INTRO, "Intro movies", "skip",
                    new String[] {"skip", "play"},
                    new String[] {"Skip", "Play"}),
            new Option(LATENCY, "Frame queue", "2",
                    new String[] {"2", "1"},
                    new String[] {"Performance", "Low latency"}),
            new Option(TEXTURES, "Texture decode", "auto",
                    new String[] {"auto", "cpu"},
                    new String[] {"Auto", "CPU"}),
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


    static String displayOverrides(Context context) {
        StringBuilder text = new StringBuilder();
        for (String[] map : DISPLAY_MAP) {
            String value = get(context, map[0]);
            if (value.equals("game")) continue;
            text.append(map[1]).append('=').append(value).append(',');
            if (map[0].equals(FOG)) text.append("g_horizonfogenable=").append(value).append(',');
        }
        return text.toString();
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
