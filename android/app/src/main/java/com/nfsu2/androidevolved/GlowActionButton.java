package com.nfsu2.androidevolved;

import android.animation.ValueAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Shader;
import android.os.SystemClock;
import android.widget.Button;

/** Title-screen-inspired pulse/sweep. Only this visible button redraws, at most 30 times/sec. */
final class GlowActionButton extends Button {
    private final Paint glow=new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint sweep=new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF edge=new RectF();
    private final Matrix position=new Matrix();
    private LinearGradient sweepGradient,textGradient;
    private boolean active,running;
    private long origin;
    private final Runnable tick=new Runnable(){public void run(){
        if(!shouldAnimate()){stop();return;}
        invalidate();postDelayed(this,34);
    }};

    GlowActionButton(Context context){
        super(context);setAllCaps(false);setTextSize(24);setTypeface(MenuTheme.font(context,true));setLetterSpacing(0);
        setMinHeight(MenuTheme.dp(context,48));setBackground(MenuTheme.action(context,true));setStateListAnimator(null);
        setTextColor(new ColorStateList(new int[][]{{-android.R.attr.state_enabled},{}},
            new int[]{context.getColor(R.color.menu_disabled),context.getColor(R.color.menu_lime)}));
        glow.setStyle(Paint.Style.STROKE);
    }
    void setActive(boolean value){active=value;sync();}
    private boolean shouldAnimate(){return active&&isAttachedToWindow()&&isShown()&&isEnabled()
        &&getWindowVisibility()==VISIBLE&&hasWindowFocus()&&ValueAnimator.areAnimatorsEnabled();}
    private void sync(){
        if(tick==null)return; // View construction may invoke lifecycle/state overrides.
        if(shouldAnimate()){if(!running){running=true;origin=SystemClock.uptimeMillis();post(tick);}}
        else stop();
    }
    private void stop(){running=false;if(tick!=null)removeCallbacks(tick);invalidate();}
    @Override protected void onAttachedToWindow(){super.onAttachedToWindow();sync();}
    @Override protected void onDetachedFromWindow(){stop();super.onDetachedFromWindow();}
    @Override protected void onVisibilityChanged(android.view.View changed,int visibility){super.onVisibilityChanged(changed,visibility);sync();}
    @Override public void onWindowFocusChanged(boolean focus){super.onWindowFocusChanged(focus);sync();}
    @Override protected void onWindowVisibilityChanged(int visibility){super.onWindowVisibilityChanged(visibility);sync();}
    @Override public void setEnabled(boolean enabled){super.setEnabled(enabled);sync();}
    @Override protected void onSizeChanged(int w,int h,int oldw,int oldh){
        super.onSizeChanged(w,h,oldw,oldh);float band=Math.max(1,w*.22f);
        sweepGradient=new LinearGradient(-band,0,band,0,new int[]{0x00A8F06A,0x30BFFF80,0x00A8F06A},null,Shader.TileMode.CLAMP);
        textGradient=new LinearGradient(-band,0,band,0,new int[]{getContext().getColor(R.color.menu_lime),0xFFF0FFD5,getContext().getColor(R.color.menu_lime)},null,Shader.TileMode.CLAMP);
        sweep.setShader(sweepGradient);
    }
    @Override protected void onDraw(Canvas canvas){
        if(!isEnabled()){super.onDraw(canvas);return;}
        float phase=running?((SystemClock.uptimeMillis()-origin)%2800)/2800f:0;
        float pulse=running?.5f-.5f*(float)Math.cos(phase*Math.PI*2):.25f;
        float inset=MenuTheme.dp(getContext(),3),radius=MenuTheme.dp(getContext(),6);
        edge.set(inset,inset,getWidth()-inset,getHeight()-inset);
        // Layered strokes give a soft halo without a software layer or full-screen blur.
        for(int i=3;i>=1;--i){glow.setStrokeWidth(MenuTheme.dp(getContext(),i*2));
            glow.setColor((int)((12+28*pulse)/i)<<24|0xA9F263);canvas.drawRoundRect(edge,radius,radius,glow);}
        glow.setStrokeWidth(MenuTheme.dp(getContext(),1.5f));glow.setColor((int)(100+145*pulse)<<24|0xC5FF86);
        canvas.drawRoundRect(edge,radius,radius,glow);
        if(running&&sweepGradient!=null){
            position.setTranslate(getWidth()*(-.35f+1.7f*phase),0);sweepGradient.setLocalMatrix(position);textGradient.setLocalMatrix(position);
            canvas.drawRoundRect(edge,radius,radius,sweep);getPaint().setShader(textGradient);
        }
        getPaint().setShadowLayer(MenuTheme.dp(getContext(),2+2*pulse),0,0,((int)(90+110*pulse)<<24)|0x8DEB42);
        super.onDraw(canvas);getPaint().setShader(null);getPaint().clearShadowLayer();
    }
}
