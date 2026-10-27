package com.nfsu2.androidevolved;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.view.View;

import java.util.Locale;

/**
 * Frame rate across the whole run: one point per quarter second, with a band from the slowest to
 * the fastest frame of that quarter. The x axis is the full run, so the line fills in as it records.
 */
public final class BenchmarkGraphView extends View {
    public static final int FIELDS = 5; // fps, min fps, max fps, render ms, draw calls
    private final float density;
    private final Paint band = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint line = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint grid = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint target = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint axis = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path bandPath = new Path(), linePath = new Path();
    private float[] points = new float[0];
    private int offset, count;
    private float seconds = 60, goalFps;

    public BenchmarkGraphView(Context context) {
        super(context);
        density = context.getResources().getDisplayMetrics().density;
        band.setColor(0x554fc3f7);
        line.setColor(0xff4fc3f7); line.setStyle(Paint.Style.STROKE); line.setStrokeWidth(1.8f * density);
        line.setStrokeJoin(Paint.Join.ROUND);
        grid.setColor(0x26ffffff); grid.setStrokeWidth(1f * density);
        target.setColor(0x99ffd54f); target.setStrokeWidth(1f * density);
        axis.setColor(0xff9aa4ad); axis.setTextSize(9.5f * density);
    }

    /** points holds FIELDS floats per quarter second, starting at offset. goalFps 0 hides the cap line. */
    public void setTimeline(float[] points, int offset, int count, float seconds, float goalFps) {
        this.points = points; this.offset = offset; this.count = count;
        this.seconds = seconds > 0 ? seconds : 60; this.goalFps = goalFps;
        invalidate();
    }

    @Override protected void onDraw(Canvas canvas) {
        float labelWidth = 24 * density;
        float left = labelWidth, right = getWidth() - 2 * density;
        float top = 4 * density, bottom = getHeight() - 13 * density;
        float peak = goalFps;
        for (int i = 0; i < count; i++) peak = Math.max(peak, points[offset + i * FIELDS + 2]);
        // A round ceiling (30, 60, 90, 120, 150...) so the scale does not jump every update.
        float scale = Math.max(30, (float) Math.ceil(peak * 1.05f / 30) * 30);
        for (int step = 0; step <= scale; step += scale > 90 ? 30 : 15) {
            float y = bottom - step / scale * (bottom - top);
            canvas.drawLine(left, y, right, y, grid);
            canvas.drawText(String.valueOf(step), 0, y + 3 * density, axis);
        }
        for (int second = 0; second <= seconds; second += 15) {
            float x = left + (right - left) * second / seconds;
            canvas.drawText(String.format(Locale.US, "%d:%02d", second / 60, second % 60), x - 8 * density, getHeight() - 1 * density, axis);
        }
        if (goalFps > 0) {
            float y = bottom - goalFps / scale * (bottom - top);
            canvas.drawLine(left, y, right, y, target);
        }
        if (count < 1) return;
        float slot = 0.25f;
        bandPath.reset(); linePath.reset();
        for (int i = 0; i < count; i++) {
            float x = left + (right - left) * Math.min(1f, (i + 0.5f) * slot / seconds);
            float y = bottom - Math.min(points[offset + i * FIELDS + 2], scale) / scale * (bottom - top);
            if (i == 0) bandPath.moveTo(x, y); else bandPath.lineTo(x, y);
        }
        for (int i = count - 1; i >= 0; i--) {
            float x = left + (right - left) * Math.min(1f, (i + 0.5f) * slot / seconds);
            float y = bottom - Math.min(points[offset + i * FIELDS + 1], scale) / scale * (bottom - top);
            bandPath.lineTo(x, y);
        }
        bandPath.close();
        canvas.drawPath(bandPath, band);
        for (int i = 0; i < count; i++) {
            float x = left + (right - left) * Math.min(1f, (i + 0.5f) * slot / seconds);
            float y = bottom - Math.min(points[offset + i * FIELDS], scale) / scale * (bottom - top);
            if (i == 0) linePath.moveTo(x, y); else linePath.lineTo(x, y);
        }
        canvas.drawPath(linePath, line);
    }
}
