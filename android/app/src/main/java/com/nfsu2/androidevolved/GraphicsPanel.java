package com.nfsu2.androidevolved;

import android.app.Dialog;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.Gravity;
import android.view.View;
import android.view.Window;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

/**
 * In-game graphics settings, reachable from the expanded dock in free roam and races.
 * The panel owns the game's display values (the native Display page is not shipped) and
 * the port's presentation options. Every row is applied to the running game; the few
 * values that can only take effect on the next launch say so.
 */
final class GraphicsPanel {
    interface Listener {
        void graphicsPause(boolean paused);
        void graphicsApply(String key, String value);
    }

    private static final String PRESET = "performance_preset";
    private static final String[][] CATEGORIES = {
            {"Performance", PRESET, GameOptions.RESOLUTION, GameOptions.FPS, GameOptions.FRAME_PACE, GameOptions.LATENCY},
            {"Effects", GameOptions.BLOOM, GameOptions.LOOK, GameOptions.SHARPEN,
                    GameOptions.TREATMENT, GameOptions.ATMOSPHERE, GameOptions.EFFECTS},
            {"Detail", GameOptions.WORLD_DETAIL, GameOptions.CAR_DETAIL, GameOptions.CAR_SHADOW,
                    GameOptions.CAR_HEADLIGHT, GameOptions.CAR_ENV_DETAIL, GameOptions.CAR_ENV_UPDATE,
                    GameOptions.ROAD_REFLECT, GameOptions.CROWD, GameOptions.LIGHT_TRAILS,
                    GameOptions.LIGHT_GLOW, GameOptions.PARTICLES, GameOptions.MOTION_BLUR,
                    GameOptions.FOG, GameOptions.DOF, GameOptions.OVERBRIGHT, GameOptions.BLEACH,
                    GameOptions.RAIN},
            {"Textures", GameOptions.HD_PACKS, GameOptions.FILTERING, GameOptions.REFLECTIONS,
                    GameOptions.TEXTURES},
            {"Display", GameOptions.WIDESCREEN, GameOptions.MINIMAP},
    };

    private final Context context;
    private final Listener listener;
    private Dialog dialog;
    private MenuPanel panel;
    private LinearLayout list;
    private LinearLayout tabs;
    private String category = CATEGORIES[0][0];

    GraphicsPanel(Context context, Listener listener) {
        this.context = context;
        this.listener = listener;
    }

    boolean isShowing() {
        return dialog != null && dialog.isShowing();
    }

    void show() {
        if (isShowing()) return;
        GameOptions.syncDisplayBundles(context);
        panel = new MenuPanel(context, "Graphics", true);
        panel.fillBody();
        LinearLayout body = panel.body();

        LinearLayout top = new LinearLayout(context);
        top.setOrientation(LinearLayout.HORIZONTAL);
        top.setGravity(Gravity.CENTER_VERTICAL);
        TextView live = new TextView(context);
        live.setText("LIVE");
        live.setTextSize(12);
        live.setTypeface(MenuTheme.font(context, true));
        live.setTextColor(MenuTheme.color(context, R.color.menu_lime));
        live.setBackground(MenuTheme.action(context, true));
        live.setGravity(Gravity.CENTER);
        live.setPadding(MenuTheme.dp(context, 10), MenuTheme.dp(context, 3),
                MenuTheme.dp(context, 10), MenuTheme.dp(context, 3));
        top.addView(live, new LinearLayout.LayoutParams(-2, -2));
        TextView hint = new TextView(context);
        hint.setText("  Applies while you drive. The game is paused while this panel is open.");
        hint.setTextSize(12);
        hint.setTextColor(MenuTheme.color(context, R.color.menu_secondary));
        hint.setTypeface(MenuTheme.font(context, false));
        top.addView(hint, new LinearLayout.LayoutParams(0, -2, 1f));
        TextView close = new TextView(context);
        close.setText("Close");
        close.setTextSize(14);
        close.setTypeface(MenuTheme.font(context, true));
        close.setTextColor(MenuTheme.color(context, R.color.menu_lime));
        close.setBackground(MenuTheme.action(context, false));
        close.setGravity(Gravity.CENTER);
        close.setPadding(MenuTheme.dp(context, 14), MenuTheme.dp(context, 4),
                MenuTheme.dp(context, 14), MenuTheme.dp(context, 4));
        close.setOnClickListener(view -> dismiss());
        top.addView(close, new LinearLayout.LayoutParams(-2, -2));
        body.addView(top, new LinearLayout.LayoutParams(-1, -2));

        tabs = new LinearLayout(context);
        tabs.setOrientation(LinearLayout.HORIZONTAL);
        tabs.setPadding(0, MenuTheme.dp(context, 6), 0, MenuTheme.dp(context, 4));
        for (String[] entry : CATEGORIES) {
            TextView tab = new TextView(context);
            tab.setText(entry[0]);
            tab.setTextSize(13);
            tab.setTypeface(MenuTheme.font(context, true));
            tab.setGravity(Gravity.CENTER);
            tab.setPadding(MenuTheme.dp(context, 10), MenuTheme.dp(context, 4),
                    MenuTheme.dp(context, 10), MenuTheme.dp(context, 4));
            tab.setOnClickListener(view -> {
                category = entry[0];
                refresh();
            });
            LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0, -2, 1f);
            params.rightMargin = MenuTheme.dp(context, 4);
            tabs.addView(tab, params);
        }
        body.addView(tabs, new LinearLayout.LayoutParams(-1, -2));

        list = new LinearLayout(context);
        list.setOrientation(LinearLayout.VERTICAL);
        ScrollView scroll = new ScrollView(context);
        scroll.setFillViewport(true);
        scroll.addView(list, new LinearLayout.LayoutParams(-1, -2));
        body.addView(scroll, new LinearLayout.LayoutParams(-1, 0, 1f));

        dialog = new Dialog(context);
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        dialog.setContentView(panel);
        dialog.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        dialog.getWindow().setDimAmount(0.55f);
        dialog.setOnDismissListener(ignored -> listener.graphicsPause(false));
        int width = Math.min(MenuTheme.dp(context, 760),
                context.getResources().getDisplayMetrics().widthPixels - MenuTheme.dp(context, 32));
        int height = context.getResources().getDisplayMetrics().heightPixels - MenuTheme.dp(context, 20);
        dialog.show();
        dialog.getWindow().setLayout(width, height);
        listener.graphicsPause(true);
        refresh();
    }

    void dismiss() {
        if (dialog != null) dialog.dismiss();
        dialog = null;
        panel = null;
        list = null;
        tabs = null;
    }

    private void refresh() {
        if (list == null) return;
        for (int i = 0; i < tabs.getChildCount(); i++) {
            TextView tab = (TextView) tabs.getChildAt(i);
            boolean selected = tab.getText().toString().equals(category);
            tab.setBackground(MenuTheme.row(context, selected));
            tab.setTextColor(MenuTheme.rowText(context, selected));
        }
        list.removeAllViews();
        for (String[] entry : CATEGORIES) {
            if (!entry[0].equals(category)) continue;
            for (int i = 1; i < entry.length; i++) addRow(entry[i]);
        }
    }

    private void addRow(String key) {
        String title, value, tag = null;
        if (PRESET.equals(key)) {
            title = "Performance preset";
            PhonePreset preset = GameOptions.phonePreset(context);
            value = preset == null ? "Custom" : preset.label;
            tag = "RES / FPS";
        } else {
            GameOptions.Option option = GameOptions.find(key);
            title = option.title;
            value = option.label(GameOptions.get(context, key));
            tag = tag(key);
        }
        LinearLayout row = new LinearLayout(context);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setMinimumHeight(MenuTheme.dp(context, 44));
        row.setPadding(MenuTheme.dp(context, 12), MenuTheme.dp(context, 6),
                MenuTheme.dp(context, 12), MenuTheme.dp(context, 6));
        row.setBackground(MenuTheme.row(context, false));
        row.setClickable(true);
        row.setFocusable(true);
        TextView name = new TextView(context);
        name.setText(title);
        name.setTextSize(16);
        name.setTypeface(MenuTheme.font(context, false));
        name.setTextColor(MenuTheme.color(context, R.color.menu_row_ink));
        name.setDuplicateParentStateEnabled(true);
        row.addView(name, new LinearLayout.LayoutParams(0, -2, 1f));
        if (tag != null) {
            TextView note = new TextView(context);
            note.setText(tag);
            note.setTextSize(10);
            note.setTypeface(MenuTheme.font(context, true));
            note.setTextColor(MenuTheme.color(context, R.color.menu_secondary));
            note.setPadding(0, 0, MenuTheme.dp(context, 8), 0);
            row.addView(note, new LinearLayout.LayoutParams(-2, -2));
        }
        TextView current = new TextView(context);
        current.setText(value);
        current.setTextSize(16);
        current.setTypeface(MenuTheme.font(context, true));
        current.setTextColor(MenuTheme.color(context, R.color.menu_lime));
        current.setDuplicateParentStateEnabled(true);
        row.addView(current, new LinearLayout.LayoutParams(-2, -2));
        row.setOnClickListener(view -> pick(key, title));
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(-1, -2);
        params.topMargin = MenuTheme.dp(context, 3);
        list.addView(row, params);
    }

    private void pick(String key, String title) {
        if (PRESET.equals(key)) {
            PhonePreset[] presets = PhonePreset.values();
            String[] labels = new String[presets.length];
            int checked = -1;
            PhonePreset current = GameOptions.phonePreset(context);
            for (int i = 0; i < presets.length; i++) {
                labels[i] = presets[i].label;
                if (presets[i] == current) checked = i;
            }
            android.app.AlertDialog.Builder builder = MenuTheme.dialog(context, title);
            MenuTheme.singleChoice(builder, context, labels, checked, (dialog, which) -> {
                dialog.dismiss();
                GameOptions.applyPhonePreset(context, presets[which]);
                listener.graphicsApply(GameOptions.RESOLUTION, presets[which].resolution);
                listener.graphicsApply(GameOptions.FPS, presets[which].fps);
                listener.graphicsApply(GameOptions.LATENCY, presets[which].backBuffers);
                refresh();
            });
            builder.setNegativeButton("Cancel", null).show();
            return;
        }
        GameOptions.Option option = GameOptions.find(key);
        int checked = 0;
        for (int i = 0; i < option.values.length; i++)
            if (option.values[i].equals(GameOptions.get(context, key))) checked = i;
        android.app.AlertDialog.Builder builder = MenuTheme.dialog(context, option.title);
        MenuTheme.singleChoice(builder, context, option.labels, checked, (dialog, which) -> {
            dialog.dismiss();
            String value = option.values[which];
            GameOptions.setDisplayBundle(context, key, value);
            GameOptions.set(context, key, value);
            listener.graphicsApply(key, value);
            refresh();
        });
        builder.setNegativeButton("Cancel", null).show();
    }

    /** Only values that wait for the next launch are called out; everything else is live. */
    private static String tag(String key) {
        if (key.equals(GameOptions.WIDESCREEN) || key.equals(GameOptions.LATENCY)
                || key.equals(GameOptions.REFLECTIONS) || key.equals(GameOptions.TEXTURES))
            return "NEXT START";
        if (key.equals(GameOptions.HD_PACKS)) return "STREAMS";
        return null;
    }
}
