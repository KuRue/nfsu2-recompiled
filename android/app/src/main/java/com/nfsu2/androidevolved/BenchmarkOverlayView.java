package com.nfsu2.androidevolved;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.view.View;

import java.util.Locale;

/** Live benchmark panel: current and average rate, 1% low, frame-time graph, and phone state. */
public final class BenchmarkOverlayView extends View {
    private final float density;
    private final Paint panel = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint title = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint big = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint label = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint value = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint line = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint guide = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint progress = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path graphPath = new Path();
    private final RectF box = new RectF();
    private float[] stats = new float[0];
    private String phone = "";
    private int targetFps;

    public BenchmarkOverlayView(Context context) {
        super(context);
        density = context.getResources().getDisplayMetrics().density;
        panel.setColor(0xc0101418);
        title.setColor(0xff9aa4ad); title.setTextSize(11 * density); title.setTypeface(Typeface.DEFAULT_BOLD);
        title.setLetterSpacing(0.12f);
        big.setColor(Color.WHITE); big.setTextSize(34 * density); big.setTypeface(Typeface.create(Typeface.MONOSPACE, Typeface.BOLD));
        label.setColor(0xff9aa4ad); label.setTextSize(11 * density);
        value.setColor(Color.WHITE); value.setTextSize(15 * density); value.setTypeface(Typeface.MONOSPACE);
        line.setColor(0xff4fc3f7); line.setStyle(Paint.Style.STROKE); line.setStrokeWidth(1.5f * density);
        guide.setColor(0x60ffffff); guide.setStrokeWidth(1f * density);
        progress.setColor(0xff4fc3f7);
    }

    /** Called on the UI thread. Layout matches deliverBenchmarkStats in main.cpp. */
    public void update(float[] values, String phoneState, int frameCap) {
        stats = values; phone = phoneState; targetFps = frameCap;
        invalidate();
    }

    @Override protected void onMeasure(int widthSpec, int heightSpec) {
        setMeasuredDimension(Math.round(260 * density), Math.round(196 * density));
    }

    @Override protected void onDraw(Canvas canvas) {
        float w = getWidth(), h = getHeight(), pad = 12 * density;
        box.set(0, 0, w, h);
        canvas.drawRoundRect(box, 10 * density, 10 * density, panel);
        if (stats.length < 7) return;
        float fps = stats[0], frameMs = stats[1], average = stats[2], low = stats[3], worst = stats[4];
        float elapsed = stats[5], total = stats[6];
        float y = pad + 11 * density;
        canvas.drawText("BENCHMARK  ·  AUTOPILOT", pad, y, title);
        y += 36 * density;
        canvas.drawText(String.format(Locale.US, "%.0f", fps), pad, y, big);
        float fpsWidth = big.measureText(String.format(Locale.US, "%.0f", fps));
        canvas.drawText("FPS", pad + fpsWidth + 6 * density, y, label);
        canvas.drawText(String.format(Locale.US, "%.1f ms", frameMs), pad + fpsWidth + 6 * density, y - 14 * density, value);
        y += 20 * density;
        float column = (w - 2 * pad) / 3;
        canvas.drawText("AVERAGE", pad, y, label);
        canvas.drawText("1% LOW", pad + column, y, label);
        canvas.drawText("WORST", pad + 2 * column, y, label);
        y += 17 * density;
        canvas.drawText(String.format(Locale.US, "%.1f", average), pad, y, value);
        canvas.drawText(String.format(Locale.US, "%.1f", low), pad + column, y, value);
        canvas.drawText(String.format(Locale.US, "%.0f ms", worst), pad + 2 * column, y, value);

        // Frame-time graph: 0 to twice the target interval, with the target as a guide line.
        float top = y + 10 * density, bottom = top + 44 * density, left = pad, right = w - pad;
        float targetMs = targetFps > 0 ? 1000f / targetFps : 1000f / 60;
        float scale = 2 * targetMs;
        float guideY = bottom - (targetMs / scale) * (bottom - top);
        canvas.drawLine(left, guideY, right, guideY, guide);
        int samples = stats.length - 7;
        if (samples > 1) {
            graphPath.reset();
            for (int i = 0; i < samples; i++) {
                float ms = Math.min(stats[7 + i], scale);
                float x = left + (right - left) * i / (BenchmarkRunCapacity - 1);
                float py = bottom - (ms / scale) * (bottom - top);
                if (i == 0) graphPath.moveTo(x, py); else graphPath.lineTo(x, py);
            }
            canvas.drawPath(graphPath, line);
        }
        y = bottom + 16 * density;
        canvas.drawText(phone, pad, y, label);
        y += 9 * density;
        float done = total > 0 ? Math.min(1f, elapsed / total) : 0;
        canvas.drawRect(pad, y, pad + (w - 2 * pad) * done, y + 3 * density, progress);
    }

    /** Matches BenchmarkRun::kRecent in frame_benchmark.h. */
    private static final int BenchmarkRunCapacity = 120;
}
