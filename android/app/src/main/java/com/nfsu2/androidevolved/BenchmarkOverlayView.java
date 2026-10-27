package com.nfsu2.androidevolved;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.view.View;
import android.widget.LinearLayout;

import java.util.Locale;

/** Live benchmark panel: headline rate, run statistics, the whole-run graph, and phone state. */
public final class BenchmarkOverlayView extends LinearLayout {
    /** Header length of the array sent by deliverBenchmarkStats in main.cpp. */
    static final int HEADER = 11;
    private final float density;
    private final Figures figures;
    private final BenchmarkGraphView graph;
    private float[] stats = new float[0];
    private String phone = "";
    private int targetFps;

    public BenchmarkOverlayView(Context context) {
        super(context);
        density = context.getResources().getDisplayMetrics().density;
        setOrientation(VERTICAL);
        int pad = Math.round(14 * density);
        setPadding(pad, Math.round(12 * density), pad, Math.round(10 * density));
        GradientDrawable background = new GradientDrawable();
        background.setColor(0xd00b0f14); background.setCornerRadius(12 * density);
        setBackground(background);
        figures = new Figures(context);
        addView(figures, new LayoutParams(Math.round(340 * density), Math.round(150 * density)));
        graph = new BenchmarkGraphView(context);
        addView(graph, new LayoutParams(Math.round(340 * density), Math.round(96 * density)));
    }

    /** Called on the UI thread. */
    public void update(float[] values, String phoneState, int frameCap, int panelHz) {
        stats = values; phone = phoneState; targetFps = frameCap;
        if (values.length >= HEADER) {
            int count = (values.length - HEADER) / BenchmarkGraphView.FIELDS;
            graph.setTimeline(values, HEADER, count, values[6], frameCap > 0 ? frameCap : panelHz);
        }
        figures.invalidate();
    }

    private final class Figures extends View {
        private final Paint title = new Paint(Paint.ANTI_ALIAS_FLAG), timer = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint big = new Paint(Paint.ANTI_ALIAS_FLAG), unit = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint label = new Paint(Paint.ANTI_ALIAS_FLAG), value = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint track = new Paint(), progress = new Paint(), rule = new Paint();

        Figures(Context context) {
            super(context);
            title.setColor(0xffe6edf3); title.setTextSize(12 * density); title.setTypeface(Typeface.DEFAULT_BOLD); title.setLetterSpacing(0.14f);
            timer.setColor(0xff9aa4ad); timer.setTextSize(12 * density); timer.setTypeface(Typeface.MONOSPACE); timer.setTextAlign(Paint.Align.RIGHT);
            big.setColor(Color.WHITE); big.setTextSize(40 * density); big.setTypeface(Typeface.create(Typeface.DEFAULT, Typeface.BOLD));
            unit.setColor(0xff9aa4ad); unit.setTextSize(12 * density);
            label.setColor(0xff8b949e); label.setTextSize(9.5f * density); label.setLetterSpacing(0.08f);
            value.setColor(Color.WHITE); value.setTextSize(16 * density); value.setTypeface(Typeface.create(Typeface.DEFAULT, Typeface.BOLD));
            track.setColor(0x33ffffff); progress.setColor(0xff4fc3f7); rule.setColor(0x22ffffff);
        }

        private void tile(Canvas canvas, float x, float y, String name, String text) {
            canvas.drawText(name, x, y, label);
            canvas.drawText(text, x, y + 19 * density, value);
        }

        @Override protected void onDraw(Canvas canvas) {
            float w = getWidth();
            float y = 12 * density;
            canvas.drawText("BENCHMARK", 0, y, title);
            if (stats.length < HEADER) return;
            float fps = stats[0], frameMs = stats[1], average = stats[2], low = stats[3], worst = stats[4];
            float elapsed = Math.min(stats[5], stats[6]), total = stats[6], calls = stats[7], render = stats[8];
            float minFps = stats[9], maxFps = stats[10];
            canvas.drawText(String.format(Locale.US, "%d:%02d / %d:%02d", (int) elapsed / 60, (int) elapsed % 60,
                    (int) total / 60, (int) total % 60), w, y, timer);
            y += 6 * density;
            canvas.drawRect(0, y, w, y + 3 * density, track);
            canvas.drawRect(0, y, w * (total > 0 ? elapsed / total : 0), y + 3 * density, progress);

            y += 44 * density;
            String rate = String.format(Locale.US, "%.0f", fps);
            canvas.drawText(rate, 0, y, big);
            float after = big.measureText(rate) + 8 * density;
            canvas.drawText("FPS", after, y - 18 * density, unit);
            canvas.drawText(String.format(Locale.US, "%.2f ms", frameMs), after, y - 2 * density, unit);
            tile(canvas, w * 0.52f, y - 26 * density, "RENDER THREAD", String.format(Locale.US, "%.2f ms", render));
            tile(canvas, w * 0.78f, y - 26 * density, "DRAW CALLS", String.format(Locale.US, "%,.0f", calls));

            y += 12 * density;
            canvas.drawRect(0, y, w, y + 1 * density, rule);
            y += 16 * density;
            float column = w / 5;
            tile(canvas, 0, y, "AVERAGE", String.format(Locale.US, "%.1f", average));
            tile(canvas, column, y, "1% LOW", String.format(Locale.US, "%.1f", low));
            tile(canvas, 2 * column, y, "MIN", String.format(Locale.US, "%.0f", minFps));
            tile(canvas, 3 * column, y, "MAX", String.format(Locale.US, "%.0f", maxFps));
            tile(canvas, 4 * column, y, "WORST", String.format(Locale.US, "%.0f ms", worst));
            y += 38 * density;
            canvas.drawText(phone + (targetFps > 0 ? "  ·  cap " + targetFps : "  ·  uncapped"), 0, y, label);
        }
    }
}
