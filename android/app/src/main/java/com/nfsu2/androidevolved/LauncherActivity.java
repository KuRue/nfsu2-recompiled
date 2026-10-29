package com.nfsu2.androidevolved;

import android.media.MediaScannerConnection;
import android.app.Dialog;
import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.Window;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Launcher in the style of the MW and Carbon ports: game files and PLAY on the left, options on the
 * right (Vulkan driver, resolution, frame cap, widescreen, compatibility).
 */
public final class LauncherActivity extends Activity {
    private static final int REQUEST_DRIVER_ZIP = 45, REQUEST_DRIVER_PROBE = 46, REQUEST_SAVE_ZIP = 47;
    private int benchmarkSeconds;
    // NFSU2 green with an amber benchmark accent.
    private int ACCENT, CYAN, ORANGE, TEXT, MUTED;
    /** {label, guest language name, LANGUAGES/<file>.bin}. */
    private static final String[][] LANGUAGES = {
        {"Español", "Spanish", "Spanish"}, {"English", "English UK", "English"},
        {"Français", "French", "French"}, {"Deutsch", "German", "German"},
        {"Italiano", "Italian", "Italian"}, {"Nederlands", "Dutch", "Dutch"},
        {"Svenska", "Swedish", "Swedish"}, {"Dansk", "Danish", "Danish"},
        {"日本語", "Japanese", "Japanese"}, {"한국어", "Korean", "Korean"},
        {"繁體中文", "Chinese (Traditional)", "Chinese"}, {"ไทย", "Thai", "Thailand"}
    };

    private LinearLayout checksList, optionsList, leftHome, leftInfo;

    /** title -> {one-line explanation, GPU cost 0-3, CPU cost 0-3}. */
    private static final java.util.Map<String, String[]> OPTION_INFO = new java.util.HashMap<>();
    static {
        OPTION_INFO.put("Benchmark", new String[] {"Runs a fixed 60-second AI race and reports frame times. Nothing changes until you apply the result.", "0", "0"});
        OPTION_INFO.put("Performance preset", new String[] {"Resolution, frame cap and buffering chosen together. Lower raises frame rate and saves heat.", "2", "1"});
        OPTION_INFO.put("Vulkan driver", new String[] {"Use the system driver or an imported Adreno driver.", "0", "0"});
        OPTION_INFO.put("Frame rate limit", new String[] {"Caps how many frames the game draws; lower runs cooler.", "1", "2"});
        OPTION_INFO.put("Frame pace", new String[] {"Even slices of the display, or the exact selected cap.", "0", "1"});
        OPTION_INFO.put("Widescreen", new String[] {"Corrected HUD and camera for wide screens.", "0", "0"});
        OPTION_INFO.put("Minimap", new String[] {"Where the radar sits on screen.", "0", "0"});
        OPTION_INFO.put("Intro", new String[] {"Play or skip the intro movies.", "0", "0"});
        OPTION_INFO.put("Texture packs", new String[] {"HD HUD and world textures. More memory and bandwidth.", "2", "0"});
        OPTION_INFO.put("Texture filtering", new String[] {"Anisotropic filtering keeps roads sharp at a distance.", "2", "0"});
        OPTION_INFO.put("Sharpening", new String[] {"Adds contrast at edges in the final pass.", "1", "0"});
        OPTION_INFO.put("Bloom and glow", new String[] {"Soft glow around bright lights and neon.", "2", "0"});
        OPTION_INFO.put("Lighting look", new String[] {"Colour grading and the game's car lighting.", "1", "1"});
        OPTION_INFO.put("Visual treatment", new String[] {"Bleach-bypass film look and depth of field.", "2", "1"});
        OPTION_INFO.put("Night effects", new String[] {"Light trails, particles, tinting and crowd.", "3", "1"});
        OPTION_INFO.put("Atmosphere", new String[] {"Fog and rain over the city.", "1", "1"});
        OPTION_INFO.put("Reflections", new String[] {"Car environment map and road reflections.", "3", "1"});
        OPTION_INFO.put("Internal resolution", new String[] {"Render scale. The biggest single cost and the biggest win for frame rate.", "3", "0"});
        OPTION_INFO.put("Level of detail preset", new String[] {"The game's own four-step display preset. Fills the individual rows below.", "3", "2"});
        OPTION_INFO.put("Display detail", new String[] {"World and car detail, shadows, effects and reflections.", "3", "2"});
        OPTION_INFO.put("World detail", new String[] {"More city geometry and objects.", "3", "2"});
        OPTION_INFO.put("Car geometry", new String[] {"Higher-detail car models.", "2", "1"});
        OPTION_INFO.put("Car shadow", new String[] {"Car shadow quality; Neon is the showiest.", "2", "0"});
        OPTION_INFO.put("Headlights", new String[] {"Headlights and their light cones.", "1", "0"});
        OPTION_INFO.put("Reflection detail", new String[] {"Size of the car reflection cube.", "3", "0"});
        OPTION_INFO.put("Reflection updates", new String[] {"Refresh the car reflections with the scene.", "2", "1"});
        OPTION_INFO.put("Road reflections", new String[] {"Wet-road reflection resolution.", "3", "0"});
        OPTION_INFO.put("Crowds", new String[] {"People on the sidewalks.", "1", "2"});
        OPTION_INFO.put("Light trails", new String[] {"The neon streaks behind cars.", "1", "0"});
        OPTION_INFO.put("Light glow", new String[] {"Glow around light sources.", "1", "0"});
        OPTION_INFO.put("Particles", new String[] {"Sparks, smoke and dust.", "2", "1"});
        OPTION_INFO.put("Motion blur", new String[] {"Speed blur while driving.", "2", "0"});
        OPTION_INFO.put("Fog", new String[] {"Distance fog and horizon haze.", "1", "0"});
        OPTION_INFO.put("Depth of field", new String[] {"Blurs far background.", "2", "0"});
        OPTION_INFO.put("Over bright", new String[] {"Brighter highlights.", "1", "0"});
        OPTION_INFO.put("Bleach bypass", new String[] {"High-contrast film look.", "1", "0"});
        OPTION_INFO.put("Rain", new String[] {"Rain over the city.", "2", "1"});
    }
    private TextView status, optionsTitle;
    private Button play, access, playArrow;
    private boolean playAfterProbe, checkingUpdates, resumed, resumeNextLaunch;
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private final Handler main = new Handler(Looper.getMainLooper());

    @Override protected void onCreate(Bundle saved) {
        super.onCreate(saved);
        ACCENT=getColor(R.color.menu_lime);CYAN=getColor(R.color.menu_ink);ORANGE=getColor(R.color.menu_warning);
        TEXT=getColor(R.color.menu_ink);MUTED=getColor(R.color.menu_secondary);
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_FULLSCREEN |
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
        setContentView(buildScreen());
        checkUpdates(false);
        String updated = ReleaseUpdates.pendingWhatsNew(this);
        if (updated != null) showWhatsNew(updated);
    }

    /** First launch after an update: the installed version's release notes from GitHub. */
    private void showWhatsNew(String version) {
        AlertDialog dialog = new AlertDialog.Builder(this, R.style.NeonDialog)
                .setTitle("Updated to version " + version + "!")
                .setMessage("Loading release notes…")
                .setPositiveButton("OK", (d, which) -> ReleaseUpdates.whatsNewSeen(this))
                .setNeutralButton("View on GitHub", (d, which) -> {
                    ReleaseUpdates.whatsNewSeen(this);
                    openUrl(Uri.parse("https://github.com/codepdbh/nfsu2-recompiled/releases/tag/v" + Uri.encode(version)));
                })
                .setOnCancelListener(d -> ReleaseUpdates.whatsNewSeen(this))
                .show();
        ReleaseUpdates.releaseNotes(this, version, notes -> {
            if (!dialog.isShowing()) return;
            // In the app, skip the download/install preamble of the release page.
            int start = notes == null ? -1 : notes.indexOf("Novedades");
            if (start > 0) notes = notes.substring(start);
            dialog.setMessage(notes != null ? notes
                    : "Release notes could not be loaded. You may be offline, or the notes may not be published yet. You can view them on GitHub.");
        });
    }

    private boolean lastGameRunning;
    private final Runnable sessionRefresh=new Runnable(){
        @Override public void run(){
            if(!resumed)return;
            if(lastGameRunning!=gameRunning())refresh();
            main.postDelayed(this,2000);
        }
    };

    @Override protected void onResume() {
        super.onResume();
        resumed = true;
        refresh();
        main.removeCallbacks(sessionRefresh);main.postDelayed(sessionRefresh,2000);
        showPendingBenchmark();
        String crash = Diagnostics.unreportedCrash(this);
        if (crash != null) {
            Diagnostics.acknowledgeCrashes(this);
            new AlertDialog.Builder(this, R.style.NeonDialog)
                    .setTitle("The game closed").setMessage(crash + ".\n\nWould you like to report this on GitHub with the full log?")
                    .setPositiveButton("Report on GitHub", (dialog, which) -> prepareReport(crash))
                    .setNegativeButton("Not now", null).show();
        }
    }

    @Override protected void onPause() {
        resumed = false;
        main.removeCallbacks(sessionRefresh);
        super.onPause();
    }

    // ---- Screen ------------------------------------------------------------------------------------------

    private View buildScreen() {
        FrameLayout screen = new FrameLayout(this);
        screen.setBackgroundColor(getColor(R.color.menu_background));
        LinearLayout columns = new LinearLayout(this);
        columns.setPadding(dp(24), dp(10), dp(24), dp(10));
        // Phones use the full width; tablets centre a readable two-pane block
        // instead of stretching the cards across the whole screen.
        int contentWidth = Math.min(getResources().getDisplayMetrics().widthPixels, dp(1200));
        screen.addView(columns, new FrameLayout.LayoutParams(contentWidth, -1, Gravity.CENTER_HORIZONTAL));

        // Left: logo, game files and PLAY.
        LinearLayout left = new LinearLayout(this);
        left.setOrientation(LinearLayout.VERTICAL);
        left.setGravity(Gravity.CENTER_HORIZONTAL);
        ScrollView leftScroll = new ScrollView(this);
        leftScroll.setFillViewport(true);
        leftScroll.addView(left);
        columns.addView(leftScroll, new LinearLayout.LayoutParams(0, -1, 1.1f));

        LinearLayout header = new LinearLayout(this);
        header.setGravity(Gravity.CENTER_VERTICAL);
        ImageView logo = new ImageView(this);
        logo.setImageResource(R.drawable.icono);
        logo.setAdjustViewBounds(true);
        header.addView(logo, new LinearLayout.LayoutParams(dp(76), dp(76)));
        LinearLayout titles = new LinearLayout(this);
        titles.setOrientation(LinearLayout.VERTICAL);
        titles.setPadding(dp(14), 0, 0, 0);
        titles.addView(label("Need for Speed Underground 2", 23, getColor(R.color.menu_white), true));
        titles.addView(label("PC game · native Android port", 13, getColor(R.color.menu_white), false));
        header.addView(titles);
        left.addView(header, new LinearLayout.LayoutParams(-1, -2));

        leftHome = new LinearLayout(this);
        leftHome.setOrientation(LinearLayout.VERTICAL);
        left.addView(leftHome, new LinearLayout.LayoutParams(-1, -2));
        leftInfo = new LinearLayout(this);
        leftInfo.setOrientation(LinearLayout.VERTICAL);
        leftInfo.setBackground(MenuTheme.panel(this));leftInfo.setPadding(dp(12),dp(10),dp(12),dp(10));
        leftInfo.setVisibility(View.GONE);
        left.addView(leftInfo, new LinearLayout.LayoutParams(-1, -2));

        LinearLayout card = card();
        LinearLayout.LayoutParams cardParams = new LinearLayout.LayoutParams(-1, -2);
        cardParams.topMargin = dp(8);
        leftHome.addView(card, cardParams);
        panelHeading(card,"Game Files");
        card.addView(MenuTheme.divider(this),new LinearLayout.LayoutParams(-1,dp(10)));
        status = label("", 13, TEXT, false);
        status.setPadding(dp(10), dp(4), dp(10), dp(6));
        card.addView(status);
        checksList = new LinearLayout(this);
        checksList.setOrientation(LinearLayout.VERTICAL);
        checksList.setBackground(MenuTheme.filePanel(this));checksList.setPadding(dp(10),dp(5),dp(10),dp(5));
        LinearLayout.LayoutParams checksParams=new LinearLayout.LayoutParams(-1,-2);
        checksParams.leftMargin=checksParams.rightMargin=dp(10);checksParams.bottomMargin=dp(4);card.addView(checksList,checksParams);

        play = actionButton("Play", true);
        play.setOnClickListener(view -> play());
        LinearLayout playRow = new LinearLayout(this);
        playRow.addView(play, new LinearLayout.LayoutParams(0, dp(54), 1f));
        playArrow = actionButton("▾", false);
        playArrow.setTextSize(TypedValue.COMPLEX_UNIT_SP, 18);
        playArrow.setLetterSpacing(0);
        LinearLayout.LayoutParams arrowParams = new LinearLayout.LayoutParams(dp(54), dp(54));
        arrowParams.leftMargin = dp(10);
        playArrow.setOnClickListener(view -> chooseStartMode());
        playRow.addView(playArrow, arrowParams);
        LinearLayout.LayoutParams playParams = new LinearLayout.LayoutParams(-1, -2);
        playParams.topMargin = dp(8);
        leftHome.addView(playRow, playParams);

        access = actionButton("Allow access to Internal storage/nfsu2", false);
        access.setOnClickListener(view -> requestAccess());
        LinearLayout.LayoutParams accessParams = new LinearLayout.LayoutParams(-1, dp(46));
        accessParams.topMargin = dp(10);
        leftHome.addView(access, accessParams);

        // Right: options.
        LinearLayout right = card();
        LinearLayout.LayoutParams rightParams = new LinearLayout.LayoutParams(0, -1, 1f);
        rightParams.leftMargin = dp(24);
        columns.addView(right, rightParams);
        optionsTitle=panelHeading(right,"Options");
        right.addView(MenuTheme.divider(this),new LinearLayout.LayoutParams(-1,dp(10)));
        TextView note = label("Applied the next time you press PLAY.", 13, MUTED, false);
        note.setPadding(dp(10), dp(2), dp(10), dp(4));
        right.addView(note);
        ScrollView scroll = new ScrollView(this);
        optionsList = new LinearLayout(this);
        optionsList.setOrientation(LinearLayout.VERTICAL);
        optionsList.setPadding(dp(10),0,dp(10),0);
        scroll.addView(optionsList);
        right.addView(scroll, new LinearLayout.LayoutParams(-1, 0, 1f));
        return screen;
    }

    private void refresh() {
        boolean allowed = allowed();
        File root = gameRoot();
        checksList.removeAllViews();
        boolean exe = allowed && new File(root, "SPEED2.EXE").isFile();
        addCheck("SPEED2.EXE", exe, true);
        StringBuilder missing = new StringBuilder();
        for (String folder : new String[] {"GLOBAL", "TRACKS", "CARS", "FRONTEND", "LANGUAGES"})
            if (!(allowed && new File(root, folder).isDirectory())) missing.append(missing.length() == 0 ? "" : ", ").append(folder);
        addCheck(missing.length() == 0 ? "Game folders" : "Missing folders: " + missing, missing.length() == 0, true);
        addCheck(allowed && new File(root, "scripts/NFSUnderground2.WidescreenFix.dat").isFile()
                ? "HUD anchors · custom table" : "HUD anchors · built-in widescreen table", true, false);
        addCheck("HD HUD pack (optional)", allowed && new File(root, "TEXPACKS/uhud.pak").isFile(), false);
        addCheck("HD world pack (optional)", allowed && new File(root, "TEXPACKS/world.pak").isFile(), false);
        boolean ready = exe && !languages().isEmpty();
        status.setText(!allowed ? "Allow file access to read Internal storage/nfsu2."
                : ready ? "Internal storage/nfsu2" : "Copy your game files to Internal storage/nfsu2.");
        // Resume only a live process. A marker alone is not a restorable snapshot.
        lastGameRunning=gameRunning();
        boolean resumable = ready && lastGameRunning;
        resumeNextLaunch = resumable;
        play.setText(resumable ? "Resume" : "Play");
        playArrow.setVisibility(resumable ? View.VISIBLE : View.GONE);
        play.setEnabled(ready);
        play.setAlpha(ready ? 1f : .45f);
        access.setVisibility(allowed ? View.GONE : View.VISIBLE);
        refreshOptions();
    }

    /** Options panel navigation: null shows the category list, otherwise the open group's key. */
    private String openGroup;
    /** The open themed value picker, if any. */
    private Dialog dialog;

    private void refreshOptions() {
        optionsTitle.setText(groupTitle(openGroup));
        optionsList.removeAllViews();
        if (openGroup != null) { buildGroup(openGroup); return; }
        PhonePreset preset = GameOptions.phonePreset(this);
        optionsList.addView(optionRow("Visual settings",
                preset == null ? "Custom" : preset.label, () -> openGroup("graphics")));
        optionsList.addView(optionRow("Controls", "Touch · triggers · vibration",
                () -> openGroup("controls")));
        optionsList.addView(optionRow("Game", languageLabel() + " · " + GameOptions.onlineServer(this),
                () -> openGroup("game")));
        optionsList.addView(optionRow("Saves", backupSummary(), () -> openGroup("saves")));
        optionsList.addView(optionRow("Support", "Updates · diagnostics",
                () -> openGroup("support")));
    }

    private String groupTitle(String key) {
        if(key==null)return "Options";
        switch(key){
            case "graphics": return "Visual settings";
            case "display": return "Display detail";
            case "controls": return "Controls";
            case "game": return "Game";
            case "saves": return "Saves";
            case "support": return "Support";
            default: return "Options";
        }
    }

    private void openGroup(String key) { openGroup = key; refreshOptions(); updateLeftPane(); }

    private void closeGroup() { openGroup = null; refreshOptions(); updateLeftPane(); }

    @Override public void onBackPressed() {
        if (openGroup != null) { closeGroup(); return; }
        super.onBackPressed();
    }

    private static boolean isControlOption(GameOptions.Option option) {
        return option.key.equals(GameOptions.FEEDBACK_SOURCE) || option.key.equals(GameOptions.FEEDBACK)
                || option.key.equals(GameOptions.PEDALS);
    }

    private void buildGroup(String key) {
        switch (key) {
            case "graphics": {
                TextView back = backRow("‹  Back");
                optionsList.addView(back);
                optionsList.addView(benchmarkRow());
                if (android.os.Process.is64Bit()) {
                    List<GpuDrivers.Driver> drivers = GpuDrivers.list(this);
                    String[] values = new String[drivers.size() + 1], labels = new String[drivers.size() + 1];
                    values[0] = ""; labels[0] = "System";
                    for (int i = 0; i < drivers.size(); i++) { values[i + 1] = drivers.get(i).id; labels[i + 1] = drivers.get(i).name; }
                    optionsList.addView(cycleRow("Vulkan driver", labels, values,
                            GpuDrivers.selectedId(this), "System", id -> { GpuDrivers.select(this, id); refreshOptions(); }));
                    if (GpuDrivers.selected(this) != null)
                        optionsList.addView(optionRow("Remove driver", GpuDrivers.selected(this).name, () -> {
                            try { GpuDrivers.remove(this, GpuDrivers.selected(this)); refreshOptions(); }
                            catch (java.io.IOException error) { message("Vulkan driver", error.getMessage()); }
                        }));
                    optionsList.addView(optionRow("Import driver ZIP", "Custom Adreno driver", () -> startActivityForResult(
                            new Intent(Intent.ACTION_OPEN_DOCUMENT).setType("*/*").addCategory(Intent.CATEGORY_OPENABLE),
                            REQUEST_DRIVER_ZIP)));
                }
                PhonePreset[] presets = PhonePreset.values();
                String[] values = new String[presets.length], labels = new String[presets.length];
                for (int i = 0; i < presets.length; i++) { values[i] = presets[i].name(); labels[i] = presets[i].label; }
                PhonePreset preset = GameOptions.phonePreset(this);
                optionsList.addView(cycleRow("Performance preset", labels, values, preset == null ? "" : preset.name(), "Custom",
                        name -> { GameOptions.applyPhonePreset(this, PhonePreset.valueOf(name)); refreshOptions(); }));
                optionsList.addView(optionRow("Display detail",
                        GameOptions.displayPresetLabel(this), () -> openGroup("display")));
                for (GameOptions.Option option : GameOptions.ALL)
                    if (!isControlOption(option) && !GameOptions.isDisplayOption(option))
                        optionsList.addView(optionCycleRow(option));
                optionsList.addView(optionRow("Run a benchmark", "60 seconds of a race, driven by the game's AI", this::confirmBenchmark));
                optionsList.addView(optionRow("Upscaling and frame generation", "Not on this GPU", this::explainUpscaling));
                optionsList.addView(optionRow("Test driver", "GPU and Vulkan features", () -> probeDriver(false)));
                optionsList.addView(optionRow("Reset graphics cache", "Rebuild shaders", () -> GraphicsCache.prompt(this)));
                break;
            }
            case "display": {
                optionsList.addView(backRow("‹  Back"));
                String[] labels = {"Low", "Medium", "High", "Ultra"};
                String[] values = {"low", "medium", "high", "ultra"};
                optionsList.addView(cycleRow("Level of detail preset", labels, values,
                        GameOptions.prefs(this).getString(GameOptions.DISPLAY_PRESET, "high"), "Low",
                        value -> { GameOptions.applyDisplayPreset(this, value); refreshOptions(); }));
                optionsList.addView(sectionDivider("INDIVIDUAL SETTINGS"));
                for (GameOptions.Option option : GameOptions.ALL)
                    if (GameOptions.isDisplayOption(option))
                        optionsList.addView(optionCycleRow(option));
                break;
            }
            case "controls":
                optionsList.addView(backRow("‹  Back"));
                for (GameOptions.Option option : GameOptions.ALL)
                    if (isControlOption(option) && !GameOptions.isDisplayOption(option))
                        optionsList.addView(optionCycleRow(option));
                optionsList.addView(optionRow("Test phone vibration", "Brief handset pulse", () -> {if(!PhoneFeedback.test(this))message("Phone vibration", "This device has no vibration motor.");}));
                break;
            case "game": {
                optionsList.addView(backRow("‹  Back"));
                List<String[]> found = allowed() ? languages() : new ArrayList<>();
                if (!found.isEmpty()) {
                    String[] values = new String[found.size()], labels = new String[found.size()];
                    for (int i = 0; i < found.size(); i++) { values[i] = found.get(i)[1]; labels[i] = found.get(i)[0]; }
                    optionsList.addView(cycleRow("Game language", labels, values, GameOptions.language(this), GameOptions.language(this), language -> {
                        GameOptions.prefs(this).edit().putString(GameOptions.LANGUAGE, language).apply(); refreshOptions();
                    }));
                }
                optionsList.addView(optionRow("Online server", GameOptions.onlineServer(this), this::chooseOnlineServer));
                break;
            }
            case "saves":
                optionsList.addView(backRow("‹  Back"));
                optionsList.addView(optionRow("Backup career saves", backupSummary(), this::backupSaves));
                optionsList.addView(optionRow("Restore career saves", "From a backup ZIP", this::restoreSaves));
                break;
            case "support":
                optionsList.addView(backRow("‹  Back"));
                optionsList.addView(optionRow("Check for updates",
                        checkingUpdates ? "Checking GitHub…" : "Version " + ReleaseUpdates.installed(this), () -> checkUpdates(true)));
                optionsList.addView(optionRow("Report a problem", "Full log for GitHub", () -> prepareReport(null)));
                break;
        }
    }

    private View optionCycleRow(GameOptions.Option option) {
        return cycleRow(option.title, option.labels, option.values, GameOptions.get(this, option.key), GameOptions.get(this, option.key), value -> {
            GameOptions.set(this, option.key, value); refreshOptions();
        });
    }

    /** A setting row that opens a themed value picker instead of cycling. */
    private View cycleRow(String title, String[] labels, String[] values, String current, String fallback,
                          java.util.function.Consumer<String> set) {
        int index = java.util.Arrays.asList(values).indexOf(current);
        return optionRow(title, index < 0 ? fallback : labels[index],
                () -> { showOptionInfo(title); chooseValue(title, labels, values, current, fallback, set); });
    }

    /** Silver menu sheet with an outlined lime selection and separate controller focus. */
    private void chooseValue(String title, String[] labels, String[] values, String current, String fallback,
                             java.util.function.Consumer<String> set) {
        LinearLayout card = card();
        card.setPadding(dp(20), dp(14), dp(20), dp(16));card.setBackground(MenuTheme.popupPanel(this));
        panelHeading(card,title);
        int index = java.util.Arrays.asList(values).indexOf(current);
        for (int i = 0; i < labels.length; i++) {
            final String value = values[i];
            LinearLayout row = new LinearLayout(this);
            row.setGravity(Gravity.CENTER_VERTICAL);
            row.setPadding(dp(12), dp(10), dp(12), dp(10));
            row.setBackground(MenuTheme.row(this,i == index));row.setMinimumHeight(dp(48));row.setFocusable(true);
            row.setClickable(true);
            row.setOnClickListener(v -> { dialog.dismiss(); set.accept(value); });
            TextView choice=label(labels[i],16,TEXT,true);choice.setTextColor(MenuTheme.rowText(this,i==index));choice.setDuplicateParentStateEnabled(true);
            row.addView(choice,
                    new LinearLayout.LayoutParams(0, -2, 1f));
            if (i == index) row.addView(label("✓", 16, ACCENT, true));
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, -2);
            lp.topMargin = dp(6);
            row.setLayoutParams(lp);
            card.addView(row);
        }
        LinearLayout footer=new LinearLayout(this);footer.setGravity(Gravity.END);footer.setPadding(dp(10),0,dp(10),dp(4));
        Button cancel=actionButton("Back",false);cancel.setOnClickListener(v->{if(dialog!=null)dialog.dismiss();});
        footer.addView(cancel,new LinearLayout.LayoutParams(dp(112),dp(48)));
        LinearLayout.LayoutParams footerParams=new LinearLayout.LayoutParams(-1,-2);footerParams.topMargin=dp(12);card.addView(footer,footerParams);
        ScrollView scroll = new ScrollView(this);
        scroll.setBackgroundColor(Color.TRANSPARENT);
        scroll.addView(card);
        final Dialog picker = new Dialog(this);
        picker.requestWindowFeature(Window.FEATURE_NO_TITLE);
        picker.setContentView(scroll);
        picker.getWindow().setBackgroundDrawable(new android.graphics.drawable.ColorDrawable(Color.TRANSPARENT));
        picker.getWindow().setDimAmount(0.6f);
        picker.show();
        picker.getWindow().setLayout(Math.min(dp(520),getResources().getDisplayMetrics().widthPixels-dp(48)),-2);
        dialog = picker;
    }

    /** Section header that returns to the category list. */
    private TextView backRow(String text) {
        TextView back = label(text,20,ACCENT,true);
        back.setBackground(MenuTheme.action(this,false));back.setMinimumHeight(dp(48));back.setFocusable(true);
        back.setGravity(Gravity.CENTER_VERTICAL);back.setPadding(dp(10),dp(4),dp(10),dp(4));
        back.setClickable(true);
        back.setOnClickListener(v -> closeGroup());
        return back;
    }

    // ---- Options ---------------------------------------------------------------------------------------

    private List<String[]> languages() {
        List<String[]> found = new ArrayList<>();
        File root = gameRoot();
        for (String[] language : LANGUAGES)
            if (new File(root, "LANGUAGES/" + language[2] + ".bin").isFile()) found.add(language);
        return found;
    }

    private String languageLabel() {
        String current = GameOptions.language(this);
        for (String[] language : LANGUAGES) if (language[1].equals(current)) return language[0];
        return current;
    }

    private void chooseOnlineServer() {
        android.widget.EditText input = new android.widget.EditText(this);
        input.setSingleLine(true);
        input.setTextColor(0xFFF2FFF0);
        input.setHintTextColor(0xFF8FA88F);
        input.setText(GameOptions.onlineServer(this));
        input.setSelection(input.getText().length());
        input.setInputType(android.text.InputType.TYPE_CLASS_TEXT | android.text.InputType.TYPE_TEXT_VARIATION_URI);
        android.widget.FrameLayout box = new android.widget.FrameLayout(this);
        int pad = (int) (20 * getResources().getDisplayMetrics().density);
        box.setPadding(pad, pad / 2, pad, 0);
        box.addView(input);
        new AlertDialog.Builder(this, R.style.NeonDialog).setTitle("Online server")
                .setMessage("The game's online mode (EA Nation) connects to this community server instead of the discontinued EA servers.")
                .setView(box)
                .setPositiveButton("Save", (dialog, which) -> {
                    String value = input.getText().toString().trim();
                    if (!GameOptions.validServer(value)) { message("Online server", "\"" + value + "\" is not a valid server name."); return; }
                    GameOptions.prefs(this).edit().putString(GameOptions.ONLINE_SERVER, value).apply();
                    refreshOptions();
                })
                .setNeutralButton("Restore default", (dialog, which) -> {
                    GameOptions.prefs(this).edit().remove(GameOptions.ONLINE_SERVER).apply();
                    refreshOptions();
                })
                .setNegativeButton("Cancel", null).show();
    }

    private void probeDriver(boolean thenPlay) {
        playAfterProbe = thenPlay;
        play.setEnabled(false);
        startActivityForResult(new Intent(this, GpuProbeActivity.class), REQUEST_DRIVER_PROBE);
    }

    private void showDriverReport(String report) {
        play.setEnabled(true);
        try {
            org.json.JSONObject gpu = new org.json.JSONObject(report);
            boolean failed = gpu.optBoolean("driverLoadFailed") || gpu.has("probeError");
            if (playAfterProbe && !failed && gpu.optBoolean("compatible")) { playAfterProbe = false; launch(); return; }
            // The player pressed PLAY: a driver that loads but lacks a feature the
            // probe checks may still run the game (issue #1107), so offer it.
            boolean wantedToPlay = playAfterProbe && !failed;
            playAfterProbe = false;
            if (!wantedToPlay) benchmarkSeconds = 0;
            String text = failed ? "Could not use the driver: " + gpu.optString("probeError", "Vulkan initialization failed.")
                    : "GPU: " + gpu.optString("gpu") + "\nVulkan: " + gpu.optString("vulkan")
                      + "\nDriver: " + gpu.optString("driverName", "—") + " " + gpu.optString("driverInfo", "")
                      + "\nBC textures: " + (gpu.optBoolean("bcTextures") ? "on GPU" : "converted on CPU")
                      + (gpu.optString("vertexFormatsMissing").isEmpty() ? ""
                              : "\nUnsupported vertex formats: " + gpu.optString("vertexFormatsMissing"))
                      + "\n" + (gpu.optBoolean("compatible") ? "Compatible with the game."
                              : "Missing features: " + gpu.optJSONArray("missing"));
            new AlertDialog.Builder(this, R.style.NeonDialog)
                    .setTitle("Vulkan driver test").setMessage(text)
                    .setPositiveButton("OK", (dialog, which) -> { if (wantedToPlay) benchmarkSeconds = 0; })
                    .setNeutralButton("Use system driver", (dialog, which) -> { benchmarkSeconds = 0; GpuDrivers.select(this, ""); refreshOptions(); })
                    .setNegativeButton(wantedToPlay ? "Play anyway" : null, wantedToPlay ? (dialog, which) -> launch() : null)
                    .setOnCancelListener(dialog -> { if (wantedToPlay) benchmarkSeconds = 0; })
                    .show();
        } catch (Exception error) { benchmarkSeconds = 0; message("Vulkan driver test", "Invalid diagnostic report: " + error.getMessage()); }
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUEST_DRIVER_PROBE) {
            showDriverReport(resultCode == RESULT_OK && data != null ? data.getStringExtra("gpu_report")
                    : "{\"probeError\":\"The test closed or was canceled.\",\"driverLoadFailed\":true}");
        } else if (requestCode == REQUEST_DRIVER_ZIP && resultCode == RESULT_OK && data != null && data.getData() != null) {
            Uri zip = data.getData();
            status.setText("Importing Vulkan driver…");
            worker.execute(() -> {
                try {
                    GpuDrivers.Driver driver = GpuDrivers.importZip(this, zip);
                    main.post(() -> {
                        GpuDrivers.select(this, driver.id); refresh();
                        status.setText("Driver imported: " + driver.name + ". Use Test driver to check it.");
                    });
                } catch (Exception error) {
                    main.post(() -> { refresh(); message("Vulkan driver", "Could not import the driver: " + error.getMessage()); });
                }
            });
        } else if (requestCode == REQUEST_SAVE_ZIP && resultCode == RESULT_OK && data != null && data.getData() != null) {
            restoreSavesFrom(data.getData());
        }
    }

    // ---- Updates and reports ---------------------------------------------------------------------------

    private void checkUpdates(boolean manual) {
        if (checkingUpdates) return;
        checkingUpdates = true;
        if (optionsList != null) refreshOptions();
        ReleaseUpdates.check(this, manual, result -> {
            checkingUpdates = false;
            if (isFinishing() || isDestroyed()) return;
            refreshOptions();
            if (result.error != null) { if (manual) message("Updates", result.error); return; }
            if (!ReleaseVersion.newer(result.tag, ReleaseUpdates.installed(this))) {
                if (manual) message("Updates", "Your version is " + ReleaseUpdates.installed(this)
                        + ". No newer version is published in " + BuildConfig.UPDATE_REPOSITORY + ".");
                return;
            }
            android.content.SharedPreferences updates = ReleaseUpdates.prefs(this);
            if (!manual && result.tag.equals(updates.getString("notified_tag", ""))) return;
            updates.edit().putString("notified_tag", result.tag).apply();
            new AlertDialog.Builder(this, R.style.NeonDialog)
                    .setTitle("New version available: " + result.tag)
                    .setMessage("Download it from GitHub or keep playing this version. Install the APK over "
                            + "the current app to preserve your saves and settings.")
                    .setPositiveButton("Update", (dialog, which) -> openUrl(Uri.parse(result.url)))
                    .setNegativeButton("Later", null).show();
        });
    }

    /**
     * Builds the report ZIP, saves it to Download/NFSU2 and opens a pre-filled GitHub issue. GitHub
     * cannot take an attachment through the link, so the issue already carries a technical summary
     * and the player is told where the ZIP is to attach it.
     */
    private void prepareReport(String crash) {
        status.setText("Preparing the log…");
        worker.execute(() -> {
            try {
                File report = Diagnostics.createReport(this);
                String location = Diagnostics.saveToDownloads(this, report);
                Uri issue = Diagnostics.issueUri(this, crash, location);
                main.post(() -> {
                    refresh();
                    new AlertDialog.Builder(this, R.style.NeonDialog)
                            .setTitle("Report on GitHub")
                            .setMessage("The full log was saved to:\n" + location
                                    + "\n\nGitHub will open with a technical summary already filled in. Describe what happened and, "
                                    + "if possible, attach that ZIP file (GitHub cannot attach it automatically).")
                            .setPositiveButton("Open GitHub", (dialog, which) -> openUrl(issue))
                            .setNegativeButton("Cancel", null).show();
                });
            } catch (Exception error) {
                main.post(() -> { refresh(); message("Report a problem", "Could not prepare the log: " + error.getMessage()); });
            }
        });
    }

    private void openUrl(Uri uri) {
        try { startActivity(new Intent(Intent.ACTION_VIEW, uri)); }
        catch (android.content.ActivityNotFoundException error) { message("GitHub", "No browser is available to open " + uri); }
    }

    // ---- Launch ----------------------------------------------------------------------------------------

    private void chooseStartMode() {
        chooseValue("How to start", new String[] {"Resume live session", "Restart game"},
                new String[] {"resume", "fresh"}, resumeNextLaunch ? "resume" : "fresh", "resume",
                mode -> {
                    if(mode.equals("resume")){resumeNextLaunch=true;launch();}
                    else new AlertDialog.Builder(this, R.style.NeonDialog).setTitle("Restart game?")
                        .setMessage("The current live session will end. Unsaved progress will be lost.")
                        .setPositiveButton("Restart",(dialog,which)->{resumeNextLaunch=false;launch();})
                        .setNegativeButton("Cancel",null).show();
                });
    }

    private void play() {
        if (!resumeNextLaunch && android.os.Process.is64Bit() && GpuDrivers.selected(this) != null) probeDriver(true);
        else launch();
    }

    private void confirmBenchmark() {
        String extra = gameRunning() ? "\n\nThe current live session will end." : "";
        new AlertDialog.Builder(this, R.style.NeonDialog)
                .setTitle("Benchmark")
                .setMessage("A fixed 60-second AI race with a live frame-time panel. You get a result and a suggestion; nothing changes until you apply it." + extra)
                .setPositiveButton("Start", (dialog, which) -> { benchmarkSeconds = 60; resumeNextLaunch = false; play(); })
                .setNegativeButton("Cancel", null).show();
    }

    private void explainUpscaling() {
        message("Upscaling and frame generation",
                "Neural upscaling and frame generation are not offered on this phone.\n\n"
                + "Qualcomm's Adreno Neural Fusion super resolution and 2× frame generation run on Snapdragon 8 Elite Gen 6 and newer. This phone's Adreno 830 is older than that. Those passes also need depth, motion vectors, and camera jitter. This 2004 game, through the Vulkan translation, does not produce that motion data. AMD FSR 4 and Arm's neural upscalers target other GPUs.\n\n"
                + "A color-only spatial upscaler could run here, but it would replace the display's own scale with a full-screen pass that has not been seen on this phone. That is not wired in.\n\n"
                + "What does change the picture: internal resolution, sharpening, texture filtering, reflections, and the frame cap. Steady pace locks the cap to an even fraction of the panel (90 FPS becomes 60 on a 120 Hz screen). The benchmark suggests the next resolution or frame cap.");
    }

    private void showPendingBenchmark() {
        File file = new File(getFilesDir(), "benchmark-report.txt");
        if (!file.isFile()) return;
        String text;
        try { text = new String(java.nio.file.Files.readAllBytes(file.toPath()), java.nio.charset.StandardCharsets.UTF_8); }
        catch (java.io.IOException error) { file.delete(); message("Benchmark", "Could not read the report."); return; }
        file.delete();
        BenchmarkReport report;
        try { report = BenchmarkReport.parse(text); }
        catch (IllegalArgumentException error) { message("Benchmark", "The report was not usable."); return; }
        if (report.averageFps > 0)
            GameOptions.prefs(this).edit().putString("benchmark_last",
                    String.format(java.util.Locale.US, "%.1f avg · %.1f 1%% low", report.averageFps, report.lowFps)).apply();
        refreshOptions();
        AlertDialog.Builder dialog = new AlertDialog.Builder(this, R.style.NeonDialog)
                .setTitle("Benchmark results").setNegativeButton(report.canApply() ? "Keep settings" : "OK", null);
        if (report.timeline.length == 0) dialog.setMessage(report.summary());
        else {
            float density = getResources().getDisplayMetrics().density;
            BenchmarkGraphView graph = new BenchmarkGraphView(this);
            int count = report.timeline.length / BenchmarkGraphView.FIELDS;
            graph.setTimeline(report.timeline, 0, count, count / 4f, report.limit > 0 ? report.limit : report.refresh);
            graph.setMinimumHeight(Math.round(130 * density));
            // Graph first, then the figures; the whole body scrolls on a short screen.
            android.widget.LinearLayout body = new android.widget.LinearLayout(this);
            body.setOrientation(android.widget.LinearLayout.VERTICAL);
            int pad = Math.round(22 * density);
            body.setPadding(pad, Math.round(8 * density), pad, 0);
            body.addView(graph, new android.widget.LinearLayout.LayoutParams(android.view.ViewGroup.LayoutParams.MATCH_PARENT, Math.round(130 * density)));
            android.widget.TextView figures = new android.widget.TextView(this);
            figures.setText(report.summary());
            figures.setTextSize(14);
            figures.setPadding(0, Math.round(12 * density), 0, 0);
            body.addView(figures);
            android.widget.ScrollView scroll = new android.widget.ScrollView(this);
            scroll.addView(body);
            dialog.setView(scroll);
        }
        if (report.canApply()) dialog.setPositiveButton("Apply", (d, which) -> {
            GameOptions.set(this, GameOptions.RESOLUTION, report.nextResolution);
            GameOptions.set(this, GameOptions.FPS, Integer.toString(report.nextFps));
            refresh();
        });
        dialog.show();
    }

    private void launch() {
        if(resumeNextLaunch){
            if(!gameRunning()){
                refresh();message("Session ended", "Android has closed the previous session. Start the game and load your career save; no disk snapshot is available.");return;
            }
            startActivity(new Intent(this,GameActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK|Intent.FLAG_ACTIVITY_REORDER_TO_FRONT));return;
        }
        android.app.ActivityManager manager=(android.app.ActivityManager)getSystemService(ACTIVITY_SERVICE);
        List<android.app.ActivityManager.RunningAppProcessInfo> processes=manager.getRunningAppProcesses();
        boolean stopped=false;
        if(processes!=null)for(android.app.ActivityManager.RunningAppProcessInfo process:processes)
            if(process.uid==android.os.Process.myUid()&&process.processName.equals(getPackageName()+":game")){
                android.os.Process.killProcess(process.pid);stopped=true;
            }
        if(stopped){main.postDelayed(this::startGame,400);return;}
        startGame();
    }

    private void startGame(){
        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getRealMetrics(metrics);
        int[] size = GameOptions.renderSize(this, Math.max(metrics.widthPixels, metrics.heightPixels),
                Math.min(metrics.widthPixels, metrics.heightPixels));
        String[] loader = GpuDrivers.loaderArguments(this);
        int seconds = benchmarkSeconds;
        benchmarkSeconds = 0;
        startActivity(new Intent(this, GameActivity.class)
                .putExtra("benchmarkSeconds", seconds)
                .putExtra("language", GameOptions.language(this))
                .putExtra("renderWidth", size[0]).putExtra("renderHeight", size[1])
                .putExtra("frameCap", Integer.parseInt(GameOptions.get(this, GameOptions.FPS)))
                .putExtra("widescreen", GameOptions.get(this, GameOptions.WIDESCREEN).equals("on"))
                .putExtra("minimapTop", GameOptions.get(this, GameOptions.MINIMAP).equals("top"))
                .putExtra("skipIntro", GameOptions.get(this, GameOptions.INTRO).equals("skip"))
                .putExtra("backBuffers", Integer.parseInt(GameOptions.get(this, GameOptions.LATENCY)))
                .putExtra("cpuTextures", GameOptions.get(this, GameOptions.TEXTURES).equals("cpu"))
                .putExtra("onlineServer", GameOptions.onlineServer(this))
                .putExtra("driverLoader", loader));
    }

    // ---- Saves -----------------------------------------------------------------------------------------

    private static File savesRoot() {
        return new File(gameRoot(), "AppData/Local/NFS Underground 2");
    }

    private static File savesBackupFolder() {
        return new File(Environment.getExternalStorageDirectory(), "Download/NFSU2/saves");
    }

    private String backupSummary() {
        if (!allowed()) return "Allow file access first";
        File[] zips = savesBackupFolder().listFiles((dir, name) -> name.endsWith(".zip"));
        return zips == null || zips.length == 0 ? "To Download/NFSU2/saves" : zips.length + " backup" + (zips.length == 1 ? "" : "s");
    }

    /** Backups copy files while the game may be writing them; refuse while it runs. */
    private boolean gameRunning() {
        android.app.ActivityManager manager = (android.app.ActivityManager) getSystemService(ACTIVITY_SERVICE);
        List<android.app.ActivityManager.RunningAppProcessInfo> processes = manager.getRunningAppProcesses();
        if (processes != null) for (android.app.ActivityManager.RunningAppProcessInfo process : processes)
            if (process.uid == android.os.Process.myUid() && process.processName.equals(getPackageName() + ":game"))
                return true;
        return false;
    }

    private void backupSaves() {
        if (!requireStorageForSaves()) return;
        if (gameRunning()) { message("Backup career saves", "The game is still running. Close it first so a save being written cannot end up in the backup."); return; }
        status.setText("Backing up career saves…");
        worker.execute(() -> {
            try {
                File zip = SaveArchive.backup(savesRoot(), savesBackupFolder());
                // Index the file so the system file picker (and other apps) can see it.
                MediaScannerConnection.scanFile(this, new String[] {zip.getPath()}, new String[] {"application/zip"}, null);
                main.post(() -> { refresh(); message("Backup career saves", "Saved " + zip.length() / 1024 + " KB to:\n" + zip.getPath()
                        + "\n\nKeep copies of this file somewhere safe, for example cloud storage."); });
            } catch (Exception error) {
                main.post(() -> { refresh(); message("Backup career saves", error.getMessage()); });
            }
        });
    }

    private void restoreSaves() {
        if (!requireStorageForSaves()) return;
        if (gameRunning()) { message("Restore career saves", "The game is still running. Close it first so an autosave cannot conflict with the restore."); return; }
        File[] zips = savesBackupFolder().listFiles((dir, name) -> name.endsWith(".zip"));
        if (zips == null || zips.length == 0) {
            openSavePicker(); return;
        }
        java.util.Arrays.sort(zips, (a, b) -> Long.compare(b.lastModified(), a.lastModified()));
        String[] names = new String[zips.length + 1];
        for (int i = 0; i < zips.length; i++)
            names[i] = zips[i].getName().replace("nfsu2-saves-", "").replace("-", " ").replace(".zip", "  (" + zips[i].length() / 1024 + " KB)");
        names[zips.length] = "Import ZIP from elsewhere…";
        new AlertDialog.Builder(this, R.style.NeonDialog)
                .setTitle("Restore career saves")
                .setItems(names, (dialog, which) -> {
                    if (which < zips.length) restoreSavesFrom(Uri.fromFile(zips[which]));
                    else openSavePicker();
                })
                .setNegativeButton("Cancel", null).show();
    }

    private void openSavePicker() {
        startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT)
                .setType("application/zip").addCategory(Intent.CATEGORY_OPENABLE), REQUEST_SAVE_ZIP);
    }

    private boolean requireStorageForSaves() {
        if (allowed()) return true;
        message("Career saves", "Allow file access first so the launcher can read Internal storage/nfsu2.");
        return false;
    }

    private void restoreSavesFrom(Uri zip) {
        try (java.io.InputStream input = getContentResolver().openInputStream(zip)) {
            if (input == null) throw new java.io.IOException("The selected file could not be opened.");
            File staged = new File(getCacheDir(), "restore-save.zip");
            java.io.OutputStream copy = new java.io.FileOutputStream(staged);
            byte[] buffer = new byte[8192];
            long total = 0;
            for (int n; (n = input.read(buffer)) != -1; ) {
                if ((total += n) > SaveArchive.MAX_BYTES) { copy.close(); staged.delete(); throw new java.io.IOException("The backup is larger than expected."); }
                copy.write(buffer, 0, n);
            }
            copy.close();
            status.setText("Restoring career saves…");
            worker.execute(() -> {
                try {
                    SaveArchive.Restored result = SaveArchive.restore(staged, savesRoot(), savesBackupFolder());
                    main.post(() -> {
                        refresh();
                        String text = "Restored " + result.files + " save file" + (result.files == 1 ? "" : "s") + ".";
                        if (result.backup != null) text += "\n\nYour previous saves were backed up first to:\n" + result.backup.getPath();
                        message("Restore career saves", text);
                    });
                } catch (Exception error) {
                    main.post(() -> { refresh(); message("Restore career saves", error.getMessage()); });
                } finally { staged.delete(); }
            });
        } catch (Exception error) {
            message("Restore career saves", "Could not read the backup: " + error.getMessage());
        }
    }

    // ---- Storage ---------------------------------------------------------------------------------------

    private static File gameRoot() { return new File(Environment.getExternalStorageDirectory(), "nfsu2"); }

    private boolean allowed() {
        return Build.VERSION.SDK_INT >= 30 ? Environment.isExternalStorageManager()
            : checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED
                && checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED;
    }

    private void requestAccess() {
        if (Build.VERSION.SDK_INT >= 30)
            startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION, Uri.parse("package:" + getPackageName())));
        else requestPermissions(new String[] {Manifest.permission.READ_EXTERNAL_STORAGE, Manifest.permission.WRITE_EXTERNAL_STORAGE}, 1);
    }

    @Override public void onRequestPermissionsResult(int code, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(code, permissions, results);
        refresh();
    }

    @Override protected void onDestroy() {
        worker.shutdown();
        super.onDestroy();
    }

    // ---- Widgets ---------------------------------------------------------------------------------------

    private void addCheck(String name, boolean ok, boolean required) {
        String text=(ok ? "✓  " : required ? "✗  " : "!  ")+name+(ok||required?"":" · optional");
        android.text.SpannableString styled=new android.text.SpannableString(text);
        styled.setSpan(new android.text.style.ForegroundColorSpan(getColor(ok?R.color.menu_success:required?R.color.menu_file_error:R.color.menu_file_warning)),0,1,android.text.Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        TextView row=label("",14,getColor(R.color.menu_white),false);row.setText(styled);
        row.setPadding(0,dp(1),0,dp(1));
        checksList.addView(row);
    }

    /** Divider with a small caption, used to separate presets from individual rows. */
    private View sectionDivider(String text) {
        LinearLayout block = new LinearLayout(this);
        block.setOrientation(LinearLayout.VERTICAL);
        block.setPadding(0, dp(14), 0, dp(2));
        block.addView(MenuTheme.divider(this), new LinearLayout.LayoutParams(-1, dp(9)));
        TextView title = label(text, 10, MUTED, true);
        title.setLetterSpacing(.14f);
        title.setPadding(dp(2), dp(8), 0, dp(2));
        block.addView(title);
        return block;
    }

    /** Contextual left pane: what the current group is, then each setting's cost. */
    private void updateLeftPane() {
        if (leftHome == null || leftInfo == null) return;
        if (openGroup == null) { leftHome.setVisibility(View.VISIBLE); leftInfo.setVisibility(View.GONE); return; }
        leftHome.setVisibility(View.GONE); leftInfo.setVisibility(View.VISIBLE);
        leftInfo.removeAllViews();
        String title=groupTitle(openGroup); String text;
        switch (openGroup) {
            case "graphics": text = "Resolution, frame rate and the game's own display effects. Changes apply the next time you press PLAY."; break;
            case "display": text = "The same switches as the game's Customize page. The preset fills them in; individual rows override it."; break;
            case "controls": text = "Touch layout, triggers and vibration."; break;
            case "game": text = "Language and the online server."; break;
            case "saves": text = "Back up or restore career saves."; break;
            case "support": text = "Updates, logs and diagnostics."; break;
            default: text = "";
        }
        panelHeading(leftInfo,title);
        TextView body = label(text, 13, MUTED, false);
        body.setPadding(dp(10), dp(8), dp(10), dp(10));
        leftInfo.addView(body);
        TextView hint = label("Tap a setting to see what it costs.", 12, MUTED, false);
        hint.setPadding(dp(10),0,dp(10),dp(6));leftInfo.addView(hint);
    }

    private void showOptionInfo(String title) {
        if (leftInfo == null || openGroup == null) return;
        String[] info = OPTION_INFO.get(title);
        String text = info != null ? info[0] : "Applies the next time you press PLAY.";
        int gpu = info != null ? Integer.parseInt(info[1]) : 0;
        int cpu = info != null ? Integer.parseInt(info[2]) : 0;
        leftInfo.removeAllViews();
        panelHeading(leftInfo,title);
        TextView body = label(text, 13, MUTED, false);
        body.setPadding(dp(10), dp(8), dp(10), dp(12));
        leftInfo.addView(body);
        addImpactBar(leftInfo, "GPU", gpu);
        addImpactBar(leftInfo, "CPU", cpu);
        if (gpu == 0 && cpu == 0) {
            TextView none = label("Negligible frame cost", 12, MUTED, false);
            none.setPadding(0, dp(6), 0, 0);
            leftInfo.addView(none);
        }
    }

    private void addImpactBar(LinearLayout parent, String name, int level) {
        LinearLayout row = new LinearLayout(this);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(dp(10), dp(3), dp(10), dp(3));
        TextView caption = label(name, 11, MUTED, true);
        caption.setLetterSpacing(.12f);
        row.addView(caption, new LinearLayout.LayoutParams(dp(40), -2));
        for (int i = 0; i < 3; i++) {
            View cell = new View(this);
            GradientDrawable bg = new GradientDrawable();
            bg.setColor(i < level ? (name.equals("GPU") ? ACCENT : ORANGE) : 0x228EE03A);
            bg.setCornerRadius(dp(3));
            cell.setBackground(bg);
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(dp(26), dp(7));
            lp.rightMargin = dp(4);
            row.addView(cell, lp);
        }
        TextView cost = label(level == 0 ? "none" : level == 1 ? "low" : level == 2 ? "medium" : "high", 11,
                level >= 3 ? ORANGE : MUTED, false);
        cost.setPadding(dp(8), 0, 0, 0);
        row.addView(cost);
        parent.addView(row);
    }

    /** Benchmark row shares the menu treatment; the last result remains underneath. */
    private View benchmarkRow() {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.VERTICAL);
        row.setPadding(dp(16), dp(12), dp(16), dp(13));
        row.setBackground(MenuTheme.row(this,false));row.setMinimumHeight(dp(48));row.setFocusable(true);
        row.setClickable(true);
        row.setOnClickListener(v -> { showOptionInfo("Benchmark"); confirmBenchmark(); });
        TextView caption=label("Benchmark",16,TEXT,true);caption.setDuplicateParentStateEnabled(true);caption.setTextColor(MenuTheme.rowText(this,false));row.addView(caption);
        String last = GameOptions.prefs(this).getString("benchmark_last", "");
        if (!last.isEmpty()) {
            TextView subtitle = label(last, 12, MUTED, false);
            subtitle.setDuplicateParentStateEnabled(true);subtitle.setTextColor(MenuTheme.rowText(this,false));
            subtitle.setPadding(0, dp(3), 0, 0);
            row.addView(subtitle);
        }
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, -2);
        lp.bottomMargin = dp(8);
        row.setLayoutParams(lp);
        return row;
    }

    private View optionRow(String title, String value, Runnable onClick) {
        return menuRow(title, value + "  ›", onClick);
    }

    private View menuRow(String title, String value, Runnable onClick) {
        LinearLayout row = new LinearLayout(this);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(dp(12), dp(8), dp(12), dp(8));
        row.setBackground(MenuTheme.row(this,false));row.setMinimumHeight(dp(48));row.setFocusable(true);
        row.setClickable(true);row.setOnClickListener(v -> onClick.run());
        TextView caption=label(title,16,TEXT,true);caption.setDuplicateParentStateEnabled(true);caption.setTextColor(MenuTheme.rowText(this,false));
        row.addView(caption,new LinearLayout.LayoutParams(0,-2,1f));
        TextView current=label(value,16,TEXT,true);current.setDuplicateParentStateEnabled(true);current.setTextColor(MenuTheme.rowText(this,false));
        current.setGravity(Gravity.END);row.addView(current,new LinearLayout.LayoutParams(0,-2,1f));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, -2);
        lp.topMargin = dp(5);
        row.setLayoutParams(lp);
        return row;
    }

    private LinearLayout card() {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(16), dp(12), dp(16), dp(12));
        card.setBackground(MenuTheme.panel(this));
        return card;
    }

    private Button actionButton(String text, boolean primary) {
        Button b = new Button(this);
        b.setText(text);
        b.setAllCaps(false);
        b.setTextColor(new android.content.res.ColorStateList(new int[][]{{-android.R.attr.state_enabled},{}},new int[]{getColor(R.color.menu_disabled),getColor(R.color.menu_white)}));
        b.setTextSize(TypedValue.COMPLEX_UNIT_SP,primary?24:18);b.setTypeface(MenuTheme.font(this,true));
        b.setLetterSpacing(0);b.setMinHeight(dp(48));b.setBackground(MenuTheme.action(this,primary));
        b.setStateListAnimator(null);
        return b;
    }

    private TextView label(String text, int sp, int color, boolean bold) {
        TextView t = new TextView(this);
        t.setText(text);
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        t.setTextColor(color);
        t.setTypeface(MenuTheme.font(this,bold));t.setLetterSpacing(0);
        return t;
    }

    /** Title occupies the panel's black top frame, rather than floating inside the gray sheet. */
    private TextView panelHeading(LinearLayout panel,String title) {
        TextView text=label(title,20,ACCENT,true);MenuTheme.heading(text);
        panel.setPadding(dp(4),dp(4),dp(4),dp(8));
        LinearLayout.LayoutParams params=new LinearLayout.LayoutParams(-1,-2);params.bottomMargin=dp(8);
        panel.addView(text,params);return text;
    }

    private void message(String title, String text) {
        new AlertDialog.Builder(this, R.style.NeonDialog)
                .setTitle(title).setMessage(text).setPositiveButton("OK", null).show();
    }

    private int dp(int value) { return Math.round(value * getResources().getDisplayMetrics().density); }
}
