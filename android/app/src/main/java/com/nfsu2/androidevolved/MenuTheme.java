package com.nfsu2.androidevolved;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.StateListDrawable;
import android.view.View;
import android.widget.TextView;

/** App-owned adaptation of the game's silver menu sheets, oblique type and lime focus frame. */
final class MenuTheme {
    private static Typeface oblique, body;
    static int color(Context c, int id) { return c.getColor(id); }
    static int dp(Context c, float value) { return Math.round(value*c.getResources().getDisplayMetrics().density); }
    static Typeface font(Context c, boolean display) {
        if (oblique == null) oblique = c.getResources().getFont(R.font.menu_oblique);
        if (body == null) body = c.getResources().getFont(R.font.menu_body);
        return display ? oblique : body;
    }
    static GradientDrawable panel(Context c) {return panel(c,R.color.menu_sheet);}
    static GradientDrawable popupPanel(Context c) {return panel(c,R.color.menu_popup_sheet);}
    private static GradientDrawable panel(Context c,int color) {
        GradientDrawable d = new GradientDrawable();
        d.setColor(color(c,color));d.setCornerRadius(dp(c,5));return d;
    }
    private static GradientDrawable surface(Context c,int fill,int stroke,int width) {
        GradientDrawable d=new GradientDrawable();d.setColor(fill);d.setCornerRadius(dp(c,6));
        if(width>0)d.setStroke(dp(c,width),stroke);return d;
    }
    static Drawable row(Context c,boolean selected) {
        StateListDrawable d=new StateListDrawable();int lime=color(c,R.color.menu_lime);
        d.addState(new int[]{-android.R.attr.state_enabled},surface(c,0x18606060,0,0));
        // White inner/action focus remains distinguishable from the persistent lime selection.
        d.addState(new int[]{android.R.attr.state_focused},surface(c,0xE0394034,color(c,R.color.menu_white),2));
        d.addState(new int[]{android.R.attr.state_pressed},surface(c,0xE0394034,lime,2));
        d.addState(new int[]{},surface(c,selected?0xD0394034:0x16FFFFFF,selected?lime:0,selected?2:0));
        return d;
    }
    static ColorStateList rowText(Context c,boolean selected) {
        return new ColorStateList(new int[][]{{-android.R.attr.state_enabled},{android.R.attr.state_focused},{android.R.attr.state_pressed},{}},
            new int[]{color(c,R.color.menu_disabled),color(c,R.color.menu_lime),color(c,R.color.menu_lime),color(c,selected?R.color.menu_lime:R.color.menu_ink)});
    }
    static Drawable action(Context c,boolean primary) {
        StateListDrawable d=new StateListDrawable();int lime=color(c,R.color.menu_lime),white=color(c,R.color.menu_white);
        d.addState(new int[]{-android.R.attr.state_enabled},surface(c,0x44101010,color(c,R.color.menu_disabled),1));
        d.addState(new int[]{android.R.attr.state_focused},surface(c,0xE02C3229,white,3));
        d.addState(new int[]{android.R.attr.state_pressed},surface(c,0xE0394034,lime,2));
        d.addState(new int[]{},surface(c,0xB50B0D0C,primary?lime:white,primary?2:1));return d;
    }
    static void heading(TextView text) {
        Context c=text.getContext();text.setTypeface(font(c,true));text.setTextSize(20);
        text.setTextColor(color(c,R.color.menu_lime));text.setLetterSpacing(0);
        text.setPadding(dp(c,10),dp(c,4),dp(c,10),dp(c,5));text.setBackgroundColor(0xDF070908);
    }
    static View rail(Context c) {
        View v=new View(c);v.setBackground(new Drawable(){
            final Paint p=new Paint(Paint.ANTI_ALIAS_FLAG);
            public void draw(Canvas canvas){float y=getBounds().exactCenterY(),h=dp(c,3);
                p.setColor(0xFF5B6359);canvas.drawRoundRect(0,y-h/2,getBounds().width(),y+h/2,h,h,p);
                p.setColor(0xFF555A53);canvas.drawCircle(dp(c,12),y,dp(c,4),p);
                p.setColor(0xFFE1E3D8);canvas.drawCircle(dp(c,12),y,dp(c,2),p);}
            public void setAlpha(int alpha) {p.setAlpha(alpha);invalidateSelf();}
            public void setColorFilter(ColorFilter f) {p.setColorFilter(f);invalidateSelf();}
            public int getOpacity(){return PixelFormat.TRANSLUCENT;}
        });return v;
    }
}
