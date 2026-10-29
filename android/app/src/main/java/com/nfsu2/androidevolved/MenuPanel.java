package com.nfsu2.androidevolved;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.widget.LinearLayout;
import android.widget.TextView;

/** A translucent black frame/title with a separate, fully rounded translucent content sheet. */
final class MenuPanel extends LinearLayout {
    private final TextView title;
    private final LinearLayout body;
    private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path frame=new Path();
    private final RectF bounds=new RectF();

    MenuPanel(Context context,String caption,boolean popup) {
        super(context);setOrientation(VERTICAL);setWillNotDraw(false);
        int border=MenuTheme.dp(context,4);setPadding(border,border,border,border);
        title=new TextView(context);title.setText(caption);MenuTheme.heading(title);
        addView(title,new LayoutParams(-1,-2));
        body=new LinearLayout(context);body.setOrientation(VERTICAL);
        body.setBackground(popup?MenuTheme.popupPanel(context):MenuTheme.panel(context));
        body.setPadding(MenuTheme.dp(context,10),MenuTheme.dp(context,8),MenuTheme.dp(context,10),MenuTheme.dp(context,8));
        LayoutParams content=new LayoutParams(-1,-2);content.topMargin=MenuTheme.dp(context,4);addView(body,content);
        paint.setColor(0xCC101010);
    }
    LinearLayout body(){return body;}
    TextView titleView(){return title;}
    void setTitle(String text){title.setText(text);}
    void fillBody(){LayoutParams p=(LayoutParams)body.getLayoutParams();p.height=0;p.weight=1;body.setLayoutParams(p);}
    @Override protected void onDraw(Canvas canvas){
        super.onDraw(canvas);
        // Leave a rounded hole behind the sheet: stacking gray over a solid black rectangle
        // would make its transparency ineffective and give its top corners the wrong shape.
        frame.rewind();frame.setFillType(Path.FillType.EVEN_ODD);
        bounds.set(0,0,getWidth(),getHeight());frame.addRoundRect(bounds,MenuTheme.dp(getContext(),10),MenuTheme.dp(getContext(),10),Path.Direction.CW);
        if(body.getWidth()>0&&body.getHeight()>0){bounds.set(body.getLeft(),body.getTop(),body.getRight(),body.getBottom());
            frame.addRoundRect(bounds,MenuTheme.dp(getContext(),6),MenuTheme.dp(getContext(),6),Path.Direction.CW);}
        canvas.drawPath(frame,paint);
    }
}
