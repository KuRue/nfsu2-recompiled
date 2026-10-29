package com.nfsu2.androidevolved;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.StateListDrawable;
import android.view.View;
import android.widget.TextView;

/** App-owned adaptation of the game's menu structure with neutral-gray sheets, oblique type and lime focus frame. */
final class MenuTheme {
    private static Typeface oblique, body;
    static int color(Context c, int id) { return c.getColor(id); }
    static int dp(Context c, float value) { return Math.round(value*c.getResources().getDisplayMetrics().density); }
    static Typeface font(Context c, boolean display) {
        if (oblique == null) oblique = c.getResources().getFont(R.font.menu_oblique);
        if (body == null) body = c.getResources().getFont(R.font.menu_body);
        return display ? oblique : body;
    }
    static Drawable panel(Context c) {return panel(c,R.color.menu_sheet);}
    static Drawable filePanel(Context c) {return surface(c,color(c,R.color.menu_file_background),0xFF626B5D,1);}
    static Drawable popupPanel(Context c) {return panel(c,R.color.menu_popup_sheet);}
    private static Drawable panel(Context c,int color) {return surface(c,color(c,color),0,0);}
    static Drawable valueBackground(Context c) {return surface(c,0xCB262A24,0,0);}
    static ColorStateList valueText(Context c) {
        return new ColorStateList(new int[][]{{-android.R.attr.state_enabled},{}},new int[]{color(c,R.color.menu_disabled),color(c,R.color.menu_lime)});
    }
    private static GradientDrawable surface(Context c,int fill,int stroke,int width) {
        GradientDrawable d=new GradientDrawable();d.setColor(fill);d.setCornerRadius(dp(c,6));
        if(width>0)d.setStroke(dp(c,width),stroke);return d;
    }
    static Drawable row(Context c,boolean selected) {
        StateListDrawable d=new StateListDrawable();int lime=color(c,R.color.menu_lime);
        d.addState(new int[]{-android.R.attr.state_enabled},surface(c,0x18606060,0,0));
        // White inner/action focus remains distinguishable from the persistent lime selection.
        d.addState(new int[]{android.R.attr.state_focused},surface(c,0xFF41463B,color(c,R.color.menu_white),2));
        d.addState(new int[]{android.R.attr.state_pressed},surface(c,0xFF41463B,lime,2));
        d.addState(new int[]{},surface(c,selected?0xFF41463B:0x00000000,selected?lime:0,selected?2:0));
        return d;
    }
    static ColorStateList rowText(Context c,boolean selected) {
        return new ColorStateList(new int[][]{{-android.R.attr.state_enabled},{android.R.attr.state_focused},{android.R.attr.state_pressed},{}},
            new int[]{color(c,R.color.menu_disabled),color(c,R.color.menu_lime),color(c,R.color.menu_lime),color(c,selected?R.color.menu_lime:R.color.menu_ink)});
    }
    static Drawable action(Context c,boolean primary) {
        StateListDrawable d=new StateListDrawable();int lime=color(c,R.color.menu_lime),white=color(c,R.color.menu_white);
        d.addState(new int[]{-android.R.attr.state_enabled},surface(c,0x44101010,color(c,R.color.menu_disabled),1));
        d.addState(new int[]{android.R.attr.state_focused},surface(c,0xFF414141,white,3));
        d.addState(new int[]{android.R.attr.state_pressed},surface(c,0xFF41463B,lime,2));
        d.addState(new int[]{},surface(c,0xFF292929,primary?lime:white,primary?2:1));return d;
    }
    static void heading(TextView text) {
        Context c=text.getContext();text.setTypeface(font(c,true));text.setTextSize(20);
        text.setTextColor(color(c,R.color.menu_lime));text.setLetterSpacing(0);
        text.setBackgroundColor(android.graphics.Color.TRANSPARENT);
        text.setPadding(dp(c,10),dp(c,4),dp(c,10),dp(c,5));text.setAccessibilityHeading(true);
    }
    /** A static header divider. Position markers belong on interactive sliders only. */
    static View divider(Context c) {
        View view=new View(c);GradientDrawable line=new GradientDrawable();
        line.setColor(0xFF777777);line.setCornerRadius(dp(c,2));
        android.graphics.drawable.InsetDrawable inset=new android.graphics.drawable.InsetDrawable(line,0,dp(c,3),0,dp(c,3));
        view.setBackground(inset);return view;
    }
}
