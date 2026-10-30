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

    private TextView status, optionsTitle, submenuBack;
    private MenuPanel infoPanel;
    /** Play/Resume. Hidden in submenus so the left pane can use the full height. */
    private LinearLayout playRow;
    private ScrollView optionsScroll;
    private GlowActionButton play;
    private Button access, playArrow;
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
        AlertDialog dialog = MenuTheme.dialog(this, "Updated to version " + version + "!")
                .setMessage("Loading release notes…")
                .setPositiveButton("OK", (d, which) -> ReleaseUpdates.whatsNewSeen(this))
                .setNeutralButton("View on GitHub", (d, which) -> {
                    ReleaseUpdates.whatsNewSeen(this);
                    openUrl(Uri.parse("https://github.com/" + BuildConfig.UPDATE_REPOSITORY + "/releases/tag/v" + Uri.encode(version)));
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
        if(play!=null)play.setActive(true);
        refresh();
        main.removeCallbacks(sessionRefresh);main.postDelayed(sessionRefresh,2000);
        showPendingBenchmark();
        String crash = Diagnostics.unreportedCrash(this);
        if (crash != null) {
            Diagnostics.acknowledgeCrashes(this);
            MenuTheme.dialog(this, "The game closed").setMessage(crash + ".\n\nWould you like to report this on GitHub with the full log?")
                    .setPositiveButton("Report on GitHub", (dialog, which) -> prepareReport(crash))
                    .setNegativeButton("Not now", null).show();
        }
    }

    @Override protected void onPause() {
        resumed = false;
        if(play!=null)play.setActive(false);
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
        LinearLayout leftColumn = new LinearLayout(this);
        leftColumn.setOrientation(LinearLayout.VERTICAL);
        columns.addView(leftColumn, new LinearLayout.LayoutParams(0, -1, 1.1f));
        ScrollView leftScroll = new ScrollView(this);
        leftScroll.setFillViewport(true);
        leftScroll.addView(left);
        leftColumn.addView(leftScroll, new LinearLayout.LayoutParams(-1, 0, 1f));

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
        titles.addView(label("Android Evolved Recomp", 13, getColor(R.color.menu_white), false));
        header.addView(titles);
        left.addView(header, new LinearLayout.LayoutParams(-1, -2));

        leftHome = new LinearLayout(this);
        leftHome.setOrientation(LinearLayout.VERTICAL);
        left.addView(leftHome, new LinearLayout.LayoutParams(-1, -2));
        infoPanel=new MenuPanel(this,"",false);leftInfo=infoPanel.body();infoPanel.setVisibility(View.GONE);
        LinearLayout.LayoutParams infoParams=new LinearLayout.LayoutParams(-1,-2);infoParams.topMargin=dp(8);
        left.addView(infoPanel,infoParams);

        MenuPanel files=new MenuPanel(this,"Game Files",false);LinearLayout card=files.body();
        LinearLayout.LayoutParams cardParams=new LinearLayout.LayoutParams(-1,-2);cardParams.topMargin=dp(8);
        leftHome.addView(files,cardParams);
        status = label("", 13, TEXT, false);
        status.setPadding(dp(10), dp(4), dp(10), dp(6));
        card.addView(status);
        checksList = new LinearLayout(this);
        checksList.setOrientation(LinearLayout.VERTICAL);
        checksList.setBackground(MenuTheme.filePanel(this));checksList.setPadding(dp(10),dp(5),dp(10),dp(5));
        LinearLayout.LayoutParams checksParams=new LinearLayout.LayoutParams(-1,-2);
        checksParams.leftMargin=checksParams.rightMargin=dp(10);checksParams.bottomMargin=dp(4);card.addView(checksList,checksParams);

        play = new GlowActionButton(this);play.setText("Play");
        play.setOnClickListener(view -> play());
        playRow = new LinearLayout(this);
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
        leftColumn.addView(playRow, playParams);

        access = actionButton("File access", false);
        access.setOnClickListener(view -> requestAccess());
        LinearLayout.LayoutParams accessParams = new LinearLayout.LayoutParams(-1, dp(46));
        accessParams.topMargin = dp(10);
        leftHome.addView(access, accessParams);

        // Header and Back remain outside the scrolling options list.
        MenuPanel right=new MenuPanel(this,"Options",false);right.fillBody();
        LinearLayout.LayoutParams rightParams=new LinearLayout.LayoutParams(0,-1,1f);rightParams.leftMargin=dp(24);
        columns.addView(right,rightParams);optionsTitle=right.titleView();
        LinearLayout optionsBody=right.body();
        submenuBack=backRow("‹  Back");submenuBack.setVisibility(View.GONE);
        LinearLayout.LayoutParams backParams=new LinearLayout.LayoutParams(-1,-2);backParams.bottomMargin=dp(8);
        optionsBody.addView(submenuBack,backParams);
        optionsScroll=new ScrollView(this);optionsList=new LinearLayout(this);optionsList.setOrientation(LinearLayout.VERTICAL);
        optionsScroll.addView(optionsList);optionsBody.addView(optionsScroll,new LinearLayout.LayoutParams(-1,0,1f));
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
        boolean resumable = ready && lastGameRunning && GameActivity.sessionMarker().isFile();
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
        updateLeftPane();
        optionsTitle.setText(groupTitle(openGroup));
        submenuBack.setVisibility(openGroup==null?View.GONE:View.VISIBLE);
        optionsList.removeAllViews();
        if (openGroup != null) { buildGroup(openGroup); return; }
        PhonePreset preset = GameOptions.phonePreset(this);
        optionsList.addView(optionRow("Graphics setup",
                preset == null ? "Custom" : preset.label, () -> openGroup("graphics")));
        optionsList.addView(optionRow("Controls", "Controller",
                () -> openGroup("controls")));
        optionsList.addView(optionRow("Game", languageLabel(),
                () -> openGroup("game")));
        optionsList.addView(optionRow("Saves", backupSummary(), () -> openGroup("saves")));
        optionsList.addView(optionRow("Support", "Diagnostics",
                () -> openGroup("support")));
    }

    private String groupTitle(String key) {
        if(key==null)return "Options";
        switch(key){
            case "graphics": return "Graphics setup";
            case "controls": return "Controls";
            case "game": return "Game";
            case "saves": return "Saves";
            case "support": return "Support";
            default: return "Options";
        }
    }

    private void openGroup(String key) { openGroup = key; refreshOptions(); updateLeftPane(); optionsScroll.scrollTo(0,0); }

    private void closeGroup() { openGroup = null; refreshOptions(); updateLeftPane(); optionsScroll.scrollTo(0,0); }

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
                    optionsList.addView(optionRow("Import driver", "ZIP file", () -> startActivityForResult(
                            new Intent(Intent.ACTION_OPEN_DOCUMENT).setType("*/*").addCategory(Intent.CATEGORY_OPENABLE),
                            REQUEST_DRIVER_ZIP)));
                }
                optionsList.addView(sectionHeader("Performance"));
                PhonePreset[] presets = PhonePreset.values();
                String[] values = new String[presets.length], labels = new String[presets.length];
                for (int i = 0; i < presets.length; i++) { values[i] = presets[i].name(); labels[i] = presets[i].label; }
                PhonePreset preset = GameOptions.phonePreset(this);
                optionsList.addView(cycleRow("Frame profile", labels, values, preset == null ? "" : preset.name(), "Custom",
                        name -> { GameOptions.applyPhonePreset(this, PhonePreset.valueOf(name)); refreshOptions(); }));
                optionsList.addView(optionCycleRow(GameOptions.find(GameOptions.RESOLUTION)));
                optionsList.addView(optionCycleRow(GameOptions.find(GameOptions.FPS)));
                optionsList.addView(optionCycleRow(GameOptions.find(GameOptions.LATENCY)));
                optionsList.addView(sectionHeader("Launch-only"));
                optionsList.addView(optionCycleRow(GameOptions.find(GameOptions.WIDESCREEN)));
                optionsList.addView(optionCycleRow(GameOptions.find(GameOptions.REFLECTIONS)));
                optionsList.addView(optionCycleRow(GameOptions.find(GameOptions.TEXTURES)));
                optionsList.addView(sectionHeader("In-game"));
                optionsList.addView(graphicsSummaryRow());
                optionsList.addView(optionRow("Upscaling", "Unavailable", this::explainUpscaling));
                optionsList.addView(optionRow("Test driver", "Vulkan features", () -> probeDriver(false)));
                optionsList.addView(optionRow("Clear shaders", "Rebuild shaders", () -> GraphicsCache.prompt(this)));
                break;
            }
            case "controls":
                for (GameOptions.Option option : GameOptions.ALL)
                    if (isControlOption(option) && !GameOptions.isDisplayOption(option))
                        optionsList.addView(optionCycleRow(option));
                optionsList.addView(optionRow("Test vibration", "Phone motor", () -> {if(!PhoneFeedback.test(this))message("Phone vibration", "This device has no vibration motor.");}));
                break;
            case "game": {
                List<String[]> found = allowed() ? languages() : new ArrayList<>();
                if (!found.isEmpty()) {
                    String[] values = new String[found.size()], labels = new String[found.size()];
                    for (int i = 0; i < found.size(); i++) { values[i] = found.get(i)[1]; labels[i] = found.get(i)[0]; }
                    optionsList.addView(cycleRow("Game language", labels, values, GameOptions.language(this), GameOptions.language(this), language -> {
                        GameOptions.prefs(this).edit().putString(GameOptions.LANGUAGE, language).apply(); refreshOptions();
                    }));
                }
                optionsList.addView(optionCycleRow(GameOptions.find(GameOptions.INTRO)));
                optionsList.addView(optionRow("Online server", GameOptions.onlineServer(this), this::chooseOnlineServer));
                break;
            }
            case "saves":
                optionsList.addView(optionRow("Backup saves", backupSummary(), this::backupSaves));
                optionsList.addView(optionRow("Restore saves", "ZIP file", this::restoreSaves));
                break;
            case "support":
                optionsList.addView(optionRow("Check updates",
                        checkingUpdates ? "Checking…" : "Version " + ReleaseUpdates.installed(this), () -> checkUpdates(true)));
                optionsList.addView(optionRow("Report issue", "Diagnostic log", () -> prepareReport(null)));
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
                () -> chooseValue(title, labels, values, current, fallback, set));
    }

    /** Silver menu sheet with an outlined lime selection and separate controller focus. */
    private void chooseValue(String title, String[] labels, String[] values, String current, String fallback,
                             java.util.function.Consumer<String> set) {
        MenuPanel pickerPanel=new MenuPanel(this,title,true);pickerPanel.fillBody();
        LinearLayout pickerBody=pickerPanel.body();
        Button cancel=actionButton("‹  Back",true);
        LinearLayout card=new LinearLayout(this);card.setOrientation(LinearLayout.VERTICAL);
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
        final Dialog picker = new Dialog(this);
        cancel.setOnClickListener(v->picker.dismiss());
        LinearLayout.LayoutParams backParams=new LinearLayout.LayoutParams(-1,-2);backParams.bottomMargin=dp(8);
        pickerBody.addView(cancel,backParams);
        ScrollView scroll=new ScrollView(this);scroll.addView(card);
        pickerBody.addView(scroll,new LinearLayout.LayoutParams(-1,0,1f));
        picker.requestWindowFeature(Window.FEATURE_NO_TITLE);picker.setContentView(pickerPanel);
        picker.getWindow().setBackgroundDrawable(new android.graphics.drawable.ColorDrawable(Color.TRANSPARENT));
        picker.getWindow().setDimAmount(0.6f);
        int width=Math.min(dp(440),getResources().getDisplayMetrics().widthPixels-dp(64));
        int available=getResources().getDisplayMetrics().heightPixels-dp(32);
        int contentWidth=View.MeasureSpec.makeMeasureSpec(width-dp(28),View.MeasureSpec.EXACTLY);
        card.measure(contentWidth,View.MeasureSpec.makeMeasureSpec(0,View.MeasureSpec.UNSPECIFIED));
        cancel.measure(contentWidth,View.MeasureSpec.makeMeasureSpec(0,View.MeasureSpec.UNSPECIFIED));
        pickerPanel.titleView().measure(View.MeasureSpec.makeMeasureSpec(width-dp(8),View.MeasureSpec.EXACTLY),View.MeasureSpec.makeMeasureSpec(0,View.MeasureSpec.UNSPECIFIED));
        int desired=card.getMeasuredHeight()+cancel.getMeasuredHeight()+pickerPanel.titleView().getMeasuredHeight()+dp(36);
        picker.show();picker.getWindow().setLayout(width,Math.min(available,desired));
        dialog = picker;
    }

    /** Section header that returns to the category list. */
    private TextView backRow(String text) {
        TextView back = label(text,20,ACCENT,true);
        back.setBackground(MenuTheme.action(this,true));back.setMinimumHeight(dp(48));back.setFocusable(true);
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
        input.setTextColor(MenuTheme.color(this,R.color.menu_ink));
        input.setHintTextColor(MenuTheme.color(this,R.color.menu_secondary));
        input.setText(GameOptions.onlineServer(this));
        input.setSelection(input.getText().length());
        input.setInputType(android.text.InputType.TYPE_CLASS_TEXT | android.text.InputType.TYPE_TEXT_VARIATION_URI);
        android.widget.FrameLayout box = new android.widget.FrameLayout(this);
        int pad = (int) (20 * getResources().getDisplayMetrics().density);
        box.setPadding(pad, pad / 2, pad, 0);
        box.addView(input);
        MenuTheme.dialog(this, "Online server")
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
            MenuTheme.dialog(this, "Vulkan driver test").setMessage(text)
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
            MenuTheme.dialog(this, "New version available: " + result.tag)
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
                android.util.Log.i("NFSU2", "Report issue: " + issue.getHost() + issue.getPath());
                main.post(() -> {
                    refresh();
                    MenuTheme.dialog(this, "Report on GitHub")
                            .setMessage("The full log was saved to:\n" + location
                                    + "\n\nGitHub will open with a technical summary already filled in. Describe what happened and, "
                                    + "if possible, attach that ZIP file (GitHub cannot attach it automatically).")
                            .setPositiveButton("Open GitHub", (dialog, which) -> openUrl(issue))
                            .setNegativeButton("Cancel", null).show();
                });
            } catch (Exception error) {
                main.post(() -> { refresh(); message("Report issue", "Could not prepare the log: " + error.getMessage()); });
            }
        });
    }

    private void openUrl(Uri uri) {
        try { startActivity(new Intent(Intent.ACTION_VIEW, uri)); }
        catch (android.content.ActivityNotFoundException error) { message("GitHub", "No browser is available to open " + uri); }
    }

    // ---- Launch ----------------------------------------------------------------------------------------

    private void chooseStartMode() {
        chooseValue("Start mode", new String[] {"Resume", "Restart"},
                new String[] {"resume", "fresh"}, resumeNextLaunch ? "resume" : "fresh", "resume",
                mode -> {
                    if(mode.equals("resume")){resumeNextLaunch=true;launch();}
                    else MenuTheme.dialog(this, "Restart game?")
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
        MenuTheme.dialog(this, "Benchmark")
                .setMessage("A fixed 60-second AI race with a live frame-time panel. You get a result and a suggestion; nothing changes until you apply it." + extra)
                .setPositiveButton("Start", (dialog, which) -> { benchmarkSeconds = 60; resumeNextLaunch = false; play(); })
                .setNegativeButton("Cancel", null).show();
    }

    private void explainUpscaling() {
        message("Upscaling",
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
        AlertDialog.Builder dialog = MenuTheme.hudDialog(this, "Benchmark results")
                .setNegativeButton(report.canApply() ? "Keep settings" : "OK", null);
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
        if (gameRunning()) { message("Backup saves", "The game is still running. Close it first so a save being written cannot end up in the backup."); return; }
        status.setText("Backing up career saves…");
        worker.execute(() -> {
            try {
                File zip = SaveArchive.backup(savesRoot(), savesBackupFolder());
                // Index the file so the system file picker (and other apps) can see it.
                MediaScannerConnection.scanFile(this, new String[] {zip.getPath()}, new String[] {"application/zip"}, null);
                main.post(() -> { refresh(); message("Backup saves", "Saved " + zip.length() / 1024 + " KB to:\n" + zip.getPath()
                        + "\n\nKeep copies of this file somewhere safe, for example cloud storage."); });
            } catch (Exception error) {
                main.post(() -> { refresh(); message("Backup saves", error.getMessage()); });
            }
        });
    }

    private void restoreSaves() {
        if (!requireStorageForSaves()) return;
        if (gameRunning()) { message("Restore saves", "The game is still running. Close it first so an autosave cannot conflict with the restore."); return; }
        File[] zips = savesBackupFolder().listFiles((dir, name) -> name.endsWith(".zip"));
        if (zips == null || zips.length == 0) {
            openSavePicker(); return;
        }
        java.util.Arrays.sort(zips, (a, b) -> Long.compare(b.lastModified(), a.lastModified()));
        String[] names = new String[zips.length + 1];
        for (int i = 0; i < zips.length; i++)
            names[i] = zips[i].getName().replace("nfsu2-saves-", "").replace("-", " ").replace(".zip", "  (" + zips[i].length() / 1024 + " KB)");
        names[zips.length] = "Import ZIP";
        AlertDialog.Builder builder = MenuTheme.dialog(this, "Restore saves");
        MenuTheme.items(builder, this, names, (dialog, which) -> {
            if (which < zips.length) restoreSavesFrom(Uri.fromFile(zips[which]));
            else openSavePicker();
        });
        builder.setNegativeButton("Cancel", null).show();
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
                        message("Restore saves", text);
                    });
                } catch (Exception error) {
                    main.post(() -> { refresh(); message("Restore saves", error.getMessage()); });
                } finally { staged.delete(); }
            });
        } catch (Exception error) {
            message("Restore saves", "Could not read the backup: " + error.getMessage());
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
        if (playRow != null) playRow.setVisibility(openGroup == null ? View.VISIBLE : View.GONE);
        if (openGroup == null) { leftHome.setVisibility(View.VISIBLE); infoPanel.setVisibility(View.GONE); return; }
        leftHome.setVisibility(View.GONE); infoPanel.setVisibility(View.VISIBLE);
        leftInfo.removeAllViews();
        String title=groupTitle(openGroup); String text;
        switch (openGroup) {
            case "graphics": text = "Resolution, frame rate and the game's own display effects."; break;
            case "display": text = "The same switches as the game's Customize page. The preset fills them in; individual rows override it."; break;
            case "controls": text = "Touch layout, triggers and vibration."; break;
            case "game": text = "Language and the online server."; break;
            case "saves": text = "Back up or restore career saves."; break;
            case "support": text = "Updates, logs and diagnostics."; break;
            default: text = "";
        }
        infoPanel.setTitle(title);
        TextView body = label(text, 13, MUTED, false);
        body.setPadding(dp(10), dp(8), dp(10), dp(10));
        leftInfo.addView(body);
        if (openGroup.equals("graphics") || openGroup.equals("display")) {
            leftInfo.removeAllViews();
            java.util.Map<String,String> settings = new java.util.HashMap<>();
            for (GameOptions.Option option : GameOptions.ALL) settings.put(option.key, GameOptions.get(this,option.key));
            android.util.DisplayMetrics metrics = new android.util.DisplayMetrics();
            getWindowManager().getDefaultDisplay().getRealMetrics(metrics);
            android.view.Display display = getWindowManager().getDefaultDisplay();
            android.view.Display.Mode current = display.getMode();
            float refresh = 60;
            for (android.view.Display.Mode mode : display.getSupportedModes())
                if (mode.getPhysicalWidth()==current.getPhysicalWidth() && mode.getPhysicalHeight()==current.getPhysicalHeight())
                    refresh=Math.max(refresh,mode.getRefreshRate());
            refresh=Math.min(120,refresh);
            GraphicsDemand demand = GraphicsDemand.estimate(settings, metrics.widthPixels, metrics.heightPixels, refresh);
            // CPU and GPU sliders share one row above the preview.
            LinearLayout sliders=new LinearLayout(this);sliders.setPadding(dp(10),dp(2),dp(10),0);
            DemandSlider cpu=new DemandSlider(this,MUTED,ACCENT,ACCENT),gpu=new DemandSlider(this,MUTED,ACCENT,ACCENT);
            cpu.set("CPU",demand.cpuScore,tierName(demand.cpuLevel));gpu.set("GPU",demand.gpuScore,tierName(demand.gpuLevel));
            LinearLayout.LayoutParams half=new LinearLayout.LayoutParams(0,-2,1f);half.rightMargin=dp(12);
            sliders.addView(cpu,half);sliders.addView(gpu,new LinearLayout.LayoutParams(0,-2,1f));
            leftInfo.addView(sliders);
            TextView target = label(settings.get(GameOptions.FPS).equals("0")
                    ? "Uncapped · "+Math.round(refresh)+" Hz reference"
                    : Math.round(demand.targetFps)+" FPS target",11,MUTED,false);
            target.setPadding(dp(10),0,dp(10),dp(4));leftInfo.addView(target);
        }
    }

    private static String tierName(int level){return level<=1?"Low":level==2?"Medium":level==3?"High":"Very high";}

    /** Benchmark row shares the menu treatment; the last result remains underneath. */
    private View benchmarkRow() {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.VERTICAL);
        row.setPadding(dp(16), dp(12), dp(16), dp(13));
        row.setBackground(MenuTheme.row(this,false));row.setMinimumHeight(dp(48));row.setFocusable(true);
        row.setClickable(true);
        row.setOnClickListener(v -> confirmBenchmark());
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

    /** Non-interactive heading inside a scrolling option group. */
    private View sectionHeader(String text) {
        TextView header = label(text, 12, ACCENT, true);
        header.setPadding(dp(10), dp(10), dp(10), dp(2));
        header.setAccessibilityHeading(true);
        return header;
    }

    /** Read-only summary of the values the in-game Graphics panel owns. */
    private View graphicsSummaryRow() {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(dp(12), dp(8), dp(12), dp(8));
        row.setBackground(MenuTheme.row(this, false));
        row.setMinimumHeight(dp(56));
        row.setFocusable(true);
        row.setClickable(true);
        row.setOnClickListener(v -> message("In-game graphics",
                "Open the expanded dock in free roam or a race and tap the monitor icon. "
                        + "Color grade, Bloom, Sharpening, HD textures, filtering, display and all scene detail are there; "
                        + "most rows apply live."));
        LinearLayout labels = new LinearLayout(this);
        labels.setOrientation(LinearLayout.VERTICAL);
        TextView title = label("Image and scene", 16, TEXT, true);
        title.setDuplicateParentStateEnabled(true);
        title.setTextColor(MenuTheme.rowText(this, false));
        labels.addView(title);
        String summary = GameOptions.find(GameOptions.LOOK).label(GameOptions.get(this, GameOptions.LOOK))
                + " · Bloom " + GameOptions.find(GameOptions.BLOOM).label(GameOptions.get(this, GameOptions.BLOOM))
                + " · World " + GameOptions.find(GameOptions.WORLD_DETAIL).label(GameOptions.get(this, GameOptions.WORLD_DETAIL));
        TextView subtitle = label(summary, 12, MUTED, false);
        subtitle.setDuplicateParentStateEnabled(true);
        subtitle.setTextColor(MenuTheme.rowText(this, false));
        subtitle.setPadding(0, dp(2), 0, 0);
        labels.addView(subtitle);
        row.addView(labels, new LinearLayout.LayoutParams(0, -2, 1f));
        TextView action = label("Change in game  ›", 16, TEXT, true);
        action.setDuplicateParentStateEnabled(true);
        action.setTextColor(MenuTheme.valueText(this));
        row.addView(action);
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
        TextView current=label(value,16,TEXT,true);current.setDuplicateParentStateEnabled(true);current.setTextColor(MenuTheme.valueText(this));current.setPadding(dp(4),dp(2),dp(4),dp(2));current.setShadowLayer(MenuTheme.dp(this,1.5f),0,MenuTheme.dp(this,.75f),0xE6000000);
        current.setGravity(Gravity.END);row.addView(current,new LinearLayout.LayoutParams(0,-2,1f));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, -2);
        lp.topMargin = dp(5);
        row.setLayoutParams(lp);
        return row;
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

    private void message(String title, String text) {
        MenuTheme.dialog(this, title).setMessage(text).setPositiveButton("OK", null).show();
    }

    private int dp(int value) { return Math.round(value * getResources().getDisplayMetrics().density); }
}
