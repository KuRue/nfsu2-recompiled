package com.nfsu2.androidevolved;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.view.View;

/**
 * Read-only demand indicator drawn like the game's own sliders: a thin rounded gray rail, a lime
 * fill and a small circular marker. The marker sits at the continuous score, so two estimates in
 * the same tier still read differently even at this size.
 */
final class DemandSlider extends View {
    /** Score at the right end of the rail: the top of the "Very high" tier. */
    static final double FULL_SCALE = 4.0;
    private final float density;
    private final Paint caption = new Paint(Paint.ANTI_ALIAS_FLAG), tier = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint rail = new Paint(Paint.ANTI_ALIAS_FLAG), fill = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint marker = new Paint(Paint.ANTI_ALIAS_FLAG), ring = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF bar = new RectF();
    private String name = "", level = "";
    private float position;

    DemandSlider(Context context, int captionColor, int valueColor, int accent) {
        super(context);
        density = context.getResources().getDisplayMetrics().density;
        caption.setColor(captionColor); caption.setTextSize(10f * density); caption.setTypeface(Typeface.DEFAULT_BOLD);
        caption.setLetterSpacing(.12f);
        tier.setColor(valueColor); tier.setTextSize(10f * density); tier.setTextAlign(Paint.Align.RIGHT);
        tier.setFakeBoldText(true); tier.setShadowLayer(1.5f * density, 0, density, 0xcc000000); // lime values carry a dark shadow, as in the menus
        rail.setColor(0x80303438); // dark enough to read on the gray settings sheet
        fill.setColor(accent);
        marker.setColor(0xfff2f5f0);
        ring.setColor(accent); ring.setStyle(Paint.Style.STROKE); ring.setStrokeWidth(1.6f * density);
    }

    /** score: GraphicsDemand's continuous estimate; label: its tier name. */
    void set(String name, double score, String label) {
        this.name = name; this.level = label;
        position = (float) Math.max(0, Math.min(1, score / FULL_SCALE));
        setContentDescription(name + " demand " + label);
        invalidate();
    }

    // One line: name, rail, tier.
    @Override protected void onMeasure(int widthSpec, int heightSpec) {
        setMeasuredDimension(MeasureSpec.getSize(widthSpec), Math.round(16 * density));
    }

    @Override protected void onDraw(Canvas canvas) {
        float y = getHeight() / 2f, baseline = y + 3.6f * density;
        canvas.drawText(name, 0, baseline, caption);
        canvas.drawText(level, getWidth(), baseline, tier);
        float radius = 4.5f * density, half = 1.4f * density;
        float left = caption.measureText(name) + 6 * density + radius;
        float right = getWidth() - tier.measureText(level) - 6 * density - radius;
        if (right <= left) return;
        bar.set(left, y - half, right, y + half);
        canvas.drawRoundRect(bar, half, half, rail);
        float x = left + (right - left) * position;
        bar.set(left, y - half, x, y + half);
        canvas.drawRoundRect(bar, half, half, fill);
        canvas.drawCircle(x, y, radius, marker);
        canvas.drawCircle(x, y, radius, ring);
    }
}
