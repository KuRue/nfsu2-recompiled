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
        band.setColor(0x338EE03A);
        line.setColor(0xff8EE03A); line.setStyle(Paint.Style.STROKE); line.setStrokeWidth(1.8f * density);
        line.setStrokeJoin(Paint.Join.ROUND);
        grid.setColor(0x26ffffff); grid.setStrokeWidth(1f * density);
        target.setColor(0xAAFFC857); target.setStrokeWidth(1f * density);
        axis.setColor(0xff8FA88F); axis.setTextSize(7.5f * density); axis.setTypeface(android.graphics.Typeface.create("sans-serif-condensed", android.graphics.Typeface.NORMAL));
    }

    /** points holds FIELDS floats per quarter second, starting at offset. goalFps 0 hides the cap line. */
    public void setTimeline(float[] points, int offset, int count, float seconds, float goalFps) {
        this.points = points; this.offset = offset; this.count = count;
        this.seconds = seconds > 0 ? seconds : 60; this.goalFps = goalFps;
        invalidate();
    }

    @Override protected void onDraw(Canvas canvas) {
        float labelWidth = 18 * density;
        float left = labelWidth, right = getWidth() - 2 * density;
        float top = 4 * density, bottom = getHeight() - 10 * density;
        float peak = goalFps;
        // Scale to the average line and the cap, not the band: one very fast frame would
        // otherwise squash the line. Round to 30s so the scale does not jump every update.
        for (int i = 0; i < count; i++) peak = Math.max(peak, points[offset + i * FIELDS]);
        float scale = Math.max(30, (float) Math.ceil(peak * 1.2f / 30) * 30);
        for (int step = 0; step <= scale; step += scale > 120 ? 60 : 30) {
            float y = bottom - step / scale * (bottom - top);
            canvas.drawLine(left, y, right, y, grid);
            canvas.drawText(String.valueOf(step), 0, y + 3 * density, axis);
        }
        for (int second = 0; second <= seconds; second += 15) {
            float x = left + (right - left) * second / seconds;
            axis.setTextAlign(second == 0 ? Paint.Align.LEFT : x > right - 12 * density ? Paint.Align.RIGHT : Paint.Align.CENTER);
            canvas.drawText(String.format(Locale.US, "%d:%02d", second / 60, second % 60), second == 0 ? x : Math.min(x, right), getHeight() - 1 * density, axis);
            axis.setTextAlign(Paint.Align.LEFT);
        }
        if (goalFps > 0) {
            float y = bottom - goalFps / scale * (bottom - top);
            canvas.drawLine(left, y, right, y, target);
        }
        if (count < 1) return;
        float slot = 0.25f;
        bandPath.reset(); linePath.reset();
        canvas.save();
        canvas.clipRect(left, top, right, bottom);
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
        canvas.restore();
    }
}
