package com.nfsu2.androidevolved;

import android.content.Context;
import android.graphics.drawable.GradientDrawable;
import android.view.Gravity;
import android.widget.TextView;
import android.widget.Toast;

/** Small themed replacement for the stock Android toast. */
final class NeonToast {
    private NeonToast() {}

    static void show(Context context, CharSequence text) {
        show(context, text, Toast.LENGTH_SHORT);
    }

    static void show(Context context, CharSequence text, int duration) {
        float density = context.getResources().getDisplayMetrics().density;
        TextView view = new TextView(context);
        view.setText(text);
        view.setTypeface(MenuTheme.font(context, false));
        view.setTextColor(0xFFF2F2F2);
        view.setTextSize(13);
        int padH = Math.round(16 * density);
        int padV = Math.round(10 * density);
        view.setPadding(padH, padV, padH, padV);
        GradientDrawable background = new GradientDrawable();
        background.setColor(0xF2101010);
        background.setCornerRadius(6 * density);
        background.setStroke(Math.max(1, Math.round(density)), MenuTheme.color(context, R.color.menu_lime));
        view.setBackground(background);

        Toast toast = new Toast(context);
        toast.setDuration(duration);
        toast.setView(view);
        toast.setGravity(Gravity.BOTTOM, 0, Math.round(96 * density));
        toast.show();
    }
}
