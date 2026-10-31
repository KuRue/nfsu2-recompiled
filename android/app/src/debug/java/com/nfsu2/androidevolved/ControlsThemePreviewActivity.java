package com.nfsu2.androidevolved;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.widget.FrameLayout;

/** Debug-only rendering fixture: exercises the real controls without booting a game or touching saves. */
public final class ControlsThemePreviewActivity extends Activity {
    @Override public void onCreate(Bundle state){
        super.onCreate(state);
        getWindow().getDecorView().setSystemUiVisibility(5894);
        FrameLayout background=new FrameLayout(this);background.setBackgroundColor(0xff424640);
        android.content.Context isolated=new android.content.ContextWrapper(this){
            @Override public android.content.SharedPreferences getSharedPreferences(String name,int mode){
                return super.getSharedPreferences("controls_theme_fixture",mode);
            }
        };
        isolated.getSharedPreferences("controls_theme_fixture",0).edit().clear()
            .putBoolean("bar.expanded",true).putString("style",getIntent().getStringExtra("style")==null?"classic":getIntent().getStringExtra("style")).apply();
        RacingControlsView controls=new RacingControlsView(isolated,
            (scan,down)->android.util.Log.i("ControlsFixture",scan+" "+down),()->{},()->{},()->{},()->{});
        background.addView(controls);setContentView(background);
        controls.post(()->controls.setGameMode(getIntent().getBooleanExtra("racing",true)));
    }
}
