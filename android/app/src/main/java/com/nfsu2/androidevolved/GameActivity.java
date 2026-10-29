package com.nfsu2.androidevolved;

import android.app.NativeActivity;
import android.os.Build;
import android.os.Environment;
import android.provider.Settings;
import android.content.Intent;
import android.net.Uri;
import android.Manifest;
import android.content.pm.PackageManager;
import android.widget.Toast;
import android.widget.TextView;
import android.os.Bundle;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.PopupWindow;
import android.graphics.Color;
import android.widget.LinearLayout;
import android.widget.EditText;
import android.widget.CheckBox;
import android.app.AlertDialog;
import android.text.InputFilter;
import android.text.InputType;
import android.view.inputmethod.EditorInfo;
import android.util.DisplayMetrics;

/** Storage permission gateway for the fixed Internal storage/nfsu2 directory. */
public final class GameActivity extends NativeActivity {
    static { System.loadLibrary("nfsu2_android"); }
    private final android.os.Handler performanceHandler=new android.os.Handler(android.os.Looper.getMainLooper());
    private final Runnable performanceSample=new Runnable() {
        @Override public void run() {
            if(!foreground)return;
            android.os.PowerManager power=getSystemService(android.os.PowerManager.class);
            Intent battery=registerReceiver(null,new android.content.IntentFilter(Intent.ACTION_BATTERY_CHANGED));
            int temperature=battery==null?0:battery.getIntExtra(android.os.BatteryManager.EXTRA_TEMPERATURE,0);
            int plugged=battery==null?0:battery.getIntExtra(android.os.BatteryManager.EXTRA_PLUGGED,0);
            android.util.Log.i("NFSU2", "Phone performance thermal="+(power==null?-1:power.getCurrentThermalStatus())+
                    " batteryC="+(temperature/10f)+" plugged="+plugged);
            performanceHandler.postDelayed(this,10000);
        }
    };
    private boolean requested;
    private boolean foreground;
    private PhoneFeedback phoneFeedback;
    private TextView status;
    private PopupWindow statusWindow;
    private PopupWindow benchmarkWindow;
    private BenchmarkOverlayView benchmarkOverlay;
    private int benchmarkCap;
    private long benchmarkPhoneRead;
    private String benchmarkPhone = "";
    private PopupWindow controlsWindow;
    private RacingControlsView touchControls;
    private boolean statusHidden;
    private static native void nativeKey(int scan, boolean down);
    private static native boolean nativeDrivingControls();
    private static native boolean nativeGamepadUsed();
    private final boolean[] touchKeys=new boolean[256],sentKeys=new boolean[256];
    private boolean tiltLeft,tiltRight,tiltEnabled,tiltCalibrate=true;
    private float tiltFiltered,tiltNeutral,tiltSensitivity=1;
    private boolean tiltInvert;
    private android.hardware.SensorManager tiltSensors;
    private android.hardware.Sensor tiltSensor;
    private final android.hardware.SensorEventListener tiltListener=new android.hardware.SensorEventListener() {
        @Override public void onAccuracyChanged(android.hardware.Sensor sensor,int accuracy) {}
        @Override public void onSensorChanged(android.hardware.SensorEvent event) {
            int rotation=getWindowManager().getDefaultDisplay().getRotation();
            float horizontal=rotation==android.view.Surface.ROTATION_90?-event.values[1]:
                    rotation==android.view.Surface.ROTATION_270?event.values[1]:event.values[0];
            if(tiltCalibrate) {tiltNeutral=horizontal;tiltFiltered=horizontal;tiltCalibrate=false;}
            tiltFiltered+=.2f*(horizontal-tiltFiltered);
            float turn=(tiltFiltered-tiltNeutral)*tiltSensitivity*(tiltInvert?-1:1);
            if(!foreground||!hasWindowFocus()||!tiltEnabled||touchControls==null||!touchControls.isDriving()) {setTiltKeys(false,false);return;}
            setTiltKeys(turn<-(tiltLeft?.5f:.8f),turn>(tiltRight?.5f:.8f));
        }
    };
    private void sendCombinedKey(int scan) {
        boolean held=touchKeys[scan]||(scan==0xcb&&tiltLeft)||(scan==0xcd&&tiltRight);
        if(sentKeys[scan]!=held) {sentKeys[scan]=held;nativeKey(scan,held);}
    }
    private void setTouchKey(int scan,boolean down) {touchKeys[scan]=down;sendCombinedKey(scan);}
    private void setTiltKeys(boolean left,boolean right) {tiltLeft=left;tiltRight=right;sendCombinedKey(0xcb);sendCombinedKey(0xcd);}
    private void updateTiltSensor() {
        if(tiltSensors==null)return;tiltSensors.unregisterListener(tiltListener);setTiltKeys(false,false);
        if(foreground&&tiltEnabled&&tiltSensor!=null) {
            tiltCalibrate=true;tiltSensors.registerListener(tiltListener,tiltSensor,android.hardware.SensorManager.SENSOR_DELAY_GAME);
        }
    }
    private void showTiltOptions() {
        if(tiltSensor==null) {NeonToast.show(this,"This device has no accelerometer",Toast.LENGTH_LONG);return;}
        android.content.SharedPreferences options=getSharedPreferences("touch_layouts_v1",MODE_PRIVATE);
        LinearLayout form=new LinearLayout(this);form.setOrientation(LinearLayout.VERTICAL);form.setPadding(dp(24),dp(8),dp(24),dp(8));
        CheckBox enabled=new CheckBox(this);enabled.setText("Steer by tilting the phone");enabled.setChecked(tiltEnabled);enabled.setTextColor(0xFFF2FFF0);enabled.setButtonTintList(android.content.res.ColorStateList.valueOf(0xFF8EE03A));form.addView(enabled);
        CheckBox invert=new CheckBox(this);invert.setText("Invert steering");invert.setChecked(tiltInvert);invert.setTextColor(0xFFF2FFF0);invert.setButtonTintList(android.content.res.ColorStateList.valueOf(0xFF8EE03A));form.addView(invert);
        TextView label=new TextView(this);label.setText("Sensitivity: "+tiltSensitivity+"×");label.setTextColor(0xFFF2FFF0);form.addView(label);
        android.widget.SeekBar sensitivity=new android.widget.SeekBar(this);sensitivity.setMax(30);sensitivity.setProgress(Math.round(tiltSensitivity*10)-10);sensitivity.setProgressTintList(android.content.res.ColorStateList.valueOf(0xFF8EE03A));sensitivity.setThumbTintList(android.content.res.ColorStateList.valueOf(0xFF8EE03A));sensitivity.setProgressBackgroundTintList(android.content.res.ColorStateList.valueOf(0x332A4A2A));form.addView(sensitivity);
        sensitivity.setOnSeekBarChangeListener(new android.widget.SeekBar.OnSeekBarChangeListener() {
            public void onStartTrackingTouch(android.widget.SeekBar bar) {}public void onStopTrackingTouch(android.widget.SeekBar bar) {}
            public void onProgressChanged(android.widget.SeekBar bar,int progress,boolean user) {label.setText("Sensitivity: "+((progress+10)/10f)+"×");}
        });
        TextView help=new TextView(this);help.setText("Hold the phone in your playing position when applying. Tilt steers left/right; the pedals remain touch controls.");help.setTextColor(0xFF8FA88F);form.addView(help);
        new AlertDialog.Builder(this, R.style.NeonDialog).setTitle("Tilt steering").setView(form)
                .setPositiveButton("Apply and calibrate",(dialog,which)->{
                    tiltEnabled=enabled.isChecked();tiltInvert=invert.isChecked();tiltSensitivity=(sensitivity.getProgress()+10)/10f;
                    options.edit().putBoolean("tilt.enabled",tiltEnabled).putBoolean("tilt.invert",tiltInvert).putFloat("tilt.sensitivity",tiltSensitivity).apply();
                    updateTiltSensor();
                }).setNegativeButton("Cancel",null).show();
    }
    private android.hardware.input.InputManager inputManager;
    private final android.hardware.input.InputManager.InputDeviceListener gamepadListener=new android.hardware.input.InputManager.InputDeviceListener() {
        @Override public void onInputDeviceAdded(int id) {updateGamepads();}
        @Override public void onInputDeviceRemoved(int id) {updateGamepads();}
        @Override public void onInputDeviceChanged(int id) {updateGamepads();}
    };
    /**
     * Physical gamepads (Bluetooth or USB) drive the game; the touch layer hides while one is connected.
     * Only external devices count: built-in sensors and side keys of several MediaTek phones report
     * themselves as joysticks with X/Y axes, and their resting value used to steer the car by itself.
     * Pads in "keyboard" mode report only KEYBOARD but still have gamepad buttons.
     */
    private void updateGamepads() {
        boolean connected=false;
        java.util.ArrayList<Integer> accepted=new java.util.ArrayList<>();
        StringBuilder seen=new StringBuilder();
        for(int id:android.view.InputDevice.getDeviceIds()) {
            android.view.InputDevice device=android.view.InputDevice.getDevice(id);
            if(device==null||device.isVirtual())continue;
            int sources=device.getSources();
            boolean buttons=(sources&android.view.InputDevice.SOURCE_GAMEPAD)==android.view.InputDevice.SOURCE_GAMEPAD;
            // Some fingerprint sensors advertise JOYSTICK with only a Z axis.
            boolean stick=(sources&android.view.InputDevice.SOURCE_JOYSTICK)==android.view.InputDevice.SOURCE_JOYSTICK&&
                    device.getMotionRange(android.view.MotionEvent.AXIS_X)!=null&&
                    device.getMotionRange(android.view.MotionEvent.AXIS_Y)!=null;
            boolean keys=(sources&android.view.InputDevice.SOURCE_KEYBOARD)==android.view.InputDevice.SOURCE_KEYBOARD&&
                    device.getKeyboardType()!=android.view.InputDevice.KEYBOARD_TYPE_ALPHABETIC&&
                    device.hasKeys(android.view.KeyEvent.KEYCODE_BUTTON_A)[0];
            boolean pad=device.isExternal()&&(buttons||stick||keys);
            seen.append(seen.length()==0?"":"; ").append(device.getName()).append(" [").append(Integer.toHexString(sources))
                    .append(device.isExternal()?" external":" internal").append(pad?" pad":"").append(']');
            if(pad){connected=true;accepted.add(id);}
        }
        android.util.Log.i("NFS","Input devices: "+seen);
        // Only these devices may move the guest's stick or press its keys.
        int[] ids=new int[accepted.size()];for(int i=0;i<ids.length;++i)ids[i]=accepted.get(i);
        nativeGamepadDevices(ids);
        if(touchControls!=null)touchControls.setGamepadConnected(connected);
        nativeGamepadConnected(connected);
    }
    private final Runnable updateControlMode = new Runnable() {
        @Override public void run() {
            if(!foreground||touchControls==null)return;
            touchControls.setGameMode(nativeDrivingControls());
            // Playing on with the pad after a touch revealed the controls hides them again.
            if(nativeGamepadUsed())touchControls.gamepadUsed();
            touchControls.postDelayed(this,250);
        }
    };
    private static native void nativeTypeText(String value, boolean replace);
    private static native void nativePointer(float x, float y, int phase);
    private static native void nativeLanguage(String language);
    private static native void nativeFeedbackSource(boolean physics);
    private static native void nativePhoneFeedback(boolean enabled);
    private static native int nativeFeedbackLevel();
    private static native void nativeStopFeedback();
    private static native void nativeTriggerFallback(boolean enabled);
    private static native void nativeDisplayOverrides(String overrides);
    private static native void nativeVisualOptions(boolean packs, int filtering, int sharpness, int look, int reflections, int bloom, int effects, int atmosphere, int treatment, int detail, boolean lockDisplay);
    private static native void nativeResolution(int width, int height);
    private static native void nativeFrameLimit(int framesPerSecond);
    private static native void nativeDisplayRefresh(int hertz);
    private static native void nativeDisplayModes(int[] hertz);
    private static native void nativeFramePace(boolean steady);
    private static native void nativeBenchmark(int seconds, String reportPath, String resolution);
    private static native void nativeWidescreen(boolean enabled);
    private static native void nativeOnlineServer(String host);
    private static native boolean nativeLanBroadcasts();
    private static native void nativeGamepadConnected(boolean connected);
    private static native void nativeGamepadDevices(int[] ids);
    private static native void nativeMinimapTop(boolean top);
    private static native void nativeSkipIntro(boolean skip);
    private static native void nativeBackBuffers(int count);
    private static native void nativeCpuTextures(boolean enabled);
    private static native void nativeGpuDriver(String hooks, String temp, String directory, String library);
    private void showFrameLimitOptions() {
        GameOptions.Option option=GameOptions.find(GameOptions.FPS);
        String saved=GameOptions.get(this,GameOptions.FPS);int selected=0;
        for(int i=0;i<option.values.length;i++)if(option.values[i].equals(saved))selected=i;
        new AlertDialog.Builder(this, R.style.NeonDialog).setTitle(option.title)
                .setSingleChoiceItems(option.labels,selected,(dialog,which)->{
                    GameOptions.set(this,GameOptions.FPS,option.values[which]);
                    nativeFrameLimit(Integer.parseInt(option.values[which]));dialog.dismiss();
                }).setNegativeButton("Cancel",null).show();
    }
    private int dp(int value) { return Math.round(value * getResources().getDisplayMetrics().density); }
    private void showTextInput() {
        EditText name = new EditText(this);
        name.setSingleLine(true);
        name.setTextColor(0xFFF2FFF0);
        name.setHintTextColor(0xFF8FA88F);
        name.setHint("Name, email, or password");
        name.setFilters(new InputFilter[]{new InputFilter.LengthFilter(40)});
        name.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD |
                InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        name.setImeOptions(EditorInfo.IME_ACTION_DONE | EditorInfo.IME_FLAG_NO_EXTRACT_UI);
        CheckBox replace = new CheckBox(this);
        replace.setText("Replace the current text");replace.setChecked(true);
        replace.setTextColor(0xFFF2FFF0);replace.setButtonTintList(android.content.res.ColorStateList.valueOf(0xFF8EE03A));
        LinearLayout form = new LinearLayout(this);form.setOrientation(LinearLayout.VERTICAL);
        form.setPadding(dp(20), 0, dp(20), 0);form.addView(name);form.addView(replace);
        AlertDialog dialog = new AlertDialog.Builder(this, R.style.NeonDialog)
                .setTitle("Type in the game")
                .setView(form)
                .setNegativeButton("Cancel", null)
                .setPositiveButton("Type", null)
                .create();
        dialog.setOnShowListener(ignored -> dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(button -> {
            String value = name.getText().toString();
            if (value.isEmpty() || !value.matches("[A-Za-z0-9 .,=/;_+?:!@#$%&*()-]{1,40}")) {
                name.setError("Up to 40 characters: unaccented letters, numbers, and . , - _ @ + ! # $ % & * ( ) = ? / ; :");
                return;
            }
            nativeTypeText(value, replace.isChecked());
            dialog.dismiss();
        }));
        dialog.show();
        name.requestFocus();
        dialog.getWindow().setSoftInputMode(android.view.WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE);
    }
    @Override protected void onCreate(Bundle saved) {
        super.onCreate(saved);
        // Legacy diagnostic marker; never treated as proof of a restorable session.
        sessionMarker().delete();
        try { new java.io.FileOutputStream(sessionMarker()).close(); } catch (java.io.IOException ignored) {}
        // Keep the paused session's process alive after the player leaves the app.
        try { startForegroundService(new Intent(this, KeepAliveService.class)); }
        catch (RuntimeException error) {android.util.Log.w("NFSU2", "Could not retain background session", error);}
        inputManager=(android.hardware.input.InputManager)getSystemService(INPUT_SERVICE);
        tiltSensors=(android.hardware.SensorManager)getSystemService(SENSOR_SERVICE);
        tiltSensor=tiltSensors==null?null:tiltSensors.getDefaultSensor(android.hardware.Sensor.TYPE_ACCELEROMETER);
        android.content.SharedPreferences options=getSharedPreferences("touch_layouts_v1",MODE_PRIVATE);
        tiltEnabled=options.getBoolean("tilt.enabled",false);tiltInvert=options.getBoolean("tilt.invert",false);
        tiltSensitivity=options.getFloat("tilt.sensitivity",1);
        getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        android.view.Display display=getWindowManager().getDefaultDisplay();
        android.view.Display.Mode current=display.getMode();float refresh=60;
        int[] found=new int[8];int modeCount=0;
        for(android.view.Display.Mode mode:display.getSupportedModes()){
            if(mode.getPhysicalWidth()!=current.getPhysicalWidth()||mode.getPhysicalHeight()!=current.getPhysicalHeight())continue;
            refresh=Math.max(refresh,mode.getRefreshRate());
            int hz=Math.round(mode.getRefreshRate());
            if(hz<30||hz>120)continue;
            boolean seen=false;
            for(int index=0;index<modeCount;index++)if(found[index]==hz)seen=true;
            if(!seen&&modeCount<found.length)found[modeCount++]=hz;
        }
        int[] rates=new int[modeCount];
        System.arraycopy(found,0,rates,0,modeCount);
        nativeDisplayModes(rates);
        nativeDisplayRefresh(Math.round(Math.min(120f, refresh)));
        nativeFramePace(GameOptions.get(this, GameOptions.FRAME_PACE).equals("steady"));
        // Launcher options travel as extras; a direct start (adb, recents) falls back to the saved ones.
        Intent launch = getIntent();
        DisplayMetrics metrics = new DisplayMetrics(); getWindowManager().getDefaultDisplay().getRealMetrics(metrics);
        int[] size = GameOptions.renderSize(this, Math.max(metrics.widthPixels, metrics.heightPixels),
                Math.min(metrics.widthPixels, metrics.heightPixels));
        nativeResolution(launch.getIntExtra("renderWidth", size[0]), launch.getIntExtra("renderHeight", size[1]));
        String language = launch.getStringExtra("language");
        nativeLanguage(language != null ? language : GameOptions.language(this));
        nativeFrameLimit(launch.getIntExtra("frameCap", Integer.parseInt(GameOptions.get(this, GameOptions.FPS))));
        int benchmarkSeconds = launch.getIntExtra("benchmarkSeconds", 0);
        if (benchmarkSeconds > 0) {
            benchmarkCap = launch.getIntExtra("frameCap", Integer.parseInt(GameOptions.get(this, GameOptions.FPS)));
            benchmarkOverlay = new BenchmarkOverlayView(this);
            benchmarkWindow = new PopupWindow(benchmarkOverlay, ViewGroup.LayoutParams.WRAP_CONTENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT, false);
            benchmarkWindow.setTouchable(false);
            benchmarkWindow.setBackgroundDrawable(new android.graphics.drawable.ColorDrawable(android.graphics.Color.TRANSPARENT));
        }
        if (benchmarkSeconds > 0) nativeBenchmark(benchmarkSeconds,
                new java.io.File(getFilesDir(), "benchmark-report.txt").getAbsolutePath(),
                GameOptions.get(this, GameOptions.RESOLUTION));
        nativeWidescreen(launch.getBooleanExtra("widescreen", GameOptions.get(this, GameOptions.WIDESCREEN).equals("on")));
        String server = launch.getStringExtra("onlineServer");
        nativeOnlineServer(GameOptions.validServer(server) ? server : GameOptions.onlineServer(this));
        nativeMinimapTop(launch.getBooleanExtra("minimapTop", GameOptions.get(this, GameOptions.MINIMAP).equals("top")));
        nativeSkipIntro(launch.getBooleanExtra("skipIntro", GameOptions.get(this, GameOptions.INTRO).equals("skip")));
        nativeBackBuffers(launch.getIntExtra("backBuffers", Integer.parseInt(GameOptions.get(this, GameOptions.LATENCY))));
        phoneFeedback=new PhoneFeedback(this,Integer.parseInt(GameOptions.get(this,GameOptions.FEEDBACK)),
                GameActivity::nativeFeedbackLevel,GameActivity::nativeStopFeedback);
        nativeFeedbackSource(GameOptions.get(this,GameOptions.FEEDBACK_SOURCE).equals("physics"));
        nativePhoneFeedback(phoneFeedback.enabled());
        nativeTriggerFallback(GameOptions.get(this, GameOptions.PEDALS).equals("triggers"));
        nativeDisplayOverrides(GameOptions.displayOverrides(this));
        nativeVisualOptions(GameOptions.get(this, GameOptions.HD_PACKS).equals("on"),
                Integer.parseInt(GameOptions.get(this, GameOptions.FILTERING)),
                Integer.parseInt(GameOptions.get(this, GameOptions.SHARPEN)),
                Integer.parseInt(GameOptions.get(this, GameOptions.LOOK)),
                Integer.parseInt(GameOptions.get(this, GameOptions.REFLECTIONS)),
                Integer.parseInt(GameOptions.get(this, GameOptions.BLOOM)),
                Integer.parseInt(GameOptions.get(this, GameOptions.EFFECTS)),
                Integer.parseInt(GameOptions.get(this, GameOptions.ATMOSPHERE)),
                Integer.parseInt(GameOptions.get(this, GameOptions.TREATMENT)),
                0, true);
        nativeCpuTextures(launch.getBooleanExtra("cpuTextures", GameOptions.get(this, GameOptions.TEXTURES).equals("cpu")));
        String[] loader = launch.getStringArrayExtra("driverLoader");
        if (loader == null || loader.length != 4) loader = GpuDrivers.loaderArguments(this);
        nativeGpuDriver(loader[0], loader[1], loader[2], loader[3]);
        status = new TextView(this);
        status.setTextColor(Color.WHITE);
        status.setBackgroundColor(0xb0000000);
        status.setTextSize(18);
        status.setPadding(24, 16, 24, 16);
        status.setText("NFSU2 Android Evolved\nChecking Internal storage/nfsu2…");
        // NativeActivity gives its main window surface to Vulkan; use a separate
        // non-interactive popup surface for the diagnostic text.
        statusWindow = new PopupWindow(status, ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT, false);
        statusWindow.setTouchable(false);
        touchControls = new RacingControlsView(this, this::setTouchKey, this::showTextInput,this::showTiltOptions,this::showFrameLimitOptions);
        touchControls.setPointer(GameActivity::nativePointer);
        controlsWindow = new PopupWindow(touchControls, ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT, false);
        controlsWindow.setClippingEnabled(false);
    }
    /** Called from the native thread. The paced cap chooses the rate. */
    public void setPreferredRefresh(int hertz) {
        runOnUiThread(() -> {
            if (Build.VERSION.SDK_INT >= 35) getWindow().setFrameRateBoostOnTouchEnabled(false);
            android.view.WindowManager.LayoutParams params = getWindow().getAttributes();
            float rate = Math.max(0, hertz);
            if (params.preferredRefreshRate == rate) return;
            params.preferredRefreshRate = rate;
            getWindow().setAttributes(params);
            android.util.Log.i("NFSU2", "Display preference " + hertz + " Hz");
        });
    }
    /** Called from the render thread four times a second while the benchmark records. */
    public void onBenchmarkStats(float[] values) {
        runOnUiThread(() -> {
            if (benchmarkOverlay == null) return;
            long now = android.os.SystemClock.uptimeMillis();
            if (now - benchmarkPhoneRead >= 1000) { benchmarkPhoneRead = now; benchmarkPhone = phoneState(); }
            android.view.Display display = getWindowManager().getDefaultDisplay();
            int panel = display != null ? Math.round(display.getMode().getRefreshRate()) : 0;
            benchmarkOverlay.update(values, benchmarkPhone, benchmarkCap, panel);
            if (!benchmarkWindow.isShowing() && hasWindowFocus())
                benchmarkWindow.showAtLocation(getWindow().getDecorView(), Gravity.TOP | Gravity.START,
                        Math.round(16 * getResources().getDisplayMetrics().density), Math.round(16 * getResources().getDisplayMetrics().density));
        });
    }
    /** Battery temperature, thermal state and panel refresh rate, for the benchmark panel. */
    private String phoneState() {
        StringBuilder text = new StringBuilder();
        Intent battery = registerReceiver(null, new android.content.IntentFilter(Intent.ACTION_BATTERY_CHANGED));
        int tenths = battery != null ? battery.getIntExtra(android.os.BatteryManager.EXTRA_TEMPERATURE, Integer.MIN_VALUE) : Integer.MIN_VALUE;
        if (tenths != Integer.MIN_VALUE) text.append(String.format(java.util.Locale.US, "%.1f °C", tenths / 10f));
        android.os.PowerManager power = (android.os.PowerManager) getSystemService(POWER_SERVICE);
        if (power != null) {
            String[] names = {"cool", "light", "moderate", "severe", "critical", "emergency", "shutdown"};
            int status = power.getCurrentThermalStatus();
            if (text.length() > 0) text.append("  ·  ");
            text.append(status >= 0 && status < names.length ? names[status] : "thermal " + status);
        }
        android.view.Display display = getWindowManager().getDefaultDisplay();
        // getRefreshRate() reports this app's frame-rate override; the mode is the panel itself.
        if (display != null) text.append(String.format(java.util.Locale.US, "  ·  panel %.0f Hz", display.getMode().getRefreshRate()));
        return text.toString();
    }
    /** Called from the guest thread once the benchmark file has been written. */
    public void onBenchmarkFinished(String path) {
        android.util.Log.i("NFSU2", "Benchmark report " + path);
        runOnUiThread(() -> {
            try { stopService(new Intent(this, KeepAliveService.class)); }
            catch (RuntimeException ignored) {}
            finish();
            android.os.Process.killProcess(android.os.Process.myPid());
        });
    }
    public void setStatus(String message) {
        runOnUiThread(() -> {
            if (message.contains("Starting game")) {
                // The game draws its own screens from here; keep the diagnostic box off them.
                statusHidden=true;
                if(statusWindow.isShowing())statusWindow.dismiss();
            } else {
                statusHidden=false;
                status.setText("NFSU2 Android Evolved\n" + message);
                if (!statusWindow.isShowing() && hasWindowFocus())
                    statusWindow.showAtLocation(getWindow().getDecorView(), Gravity.TOP | Gravity.START, 0, 0);
            }
        });
    }
    @Override public void onWindowFocusChanged(boolean focused) {
        super.onWindowFocusChanged(focused);
        if(!focused) {
            if(touchControls!=null)touchControls.releaseAll();
            setTiltKeys(false,false);
            if(controlsWindow!=null)controlsWindow.dismiss();
        }
        if(focused){
            if(Build.VERSION.SDK_INT>=30){
                android.view.WindowInsetsController insets=getWindow().getInsetsController();
                if(insets!=null){insets.hide(android.view.WindowInsets.Type.systemBars());
                    insets.setSystemBarsBehavior(android.view.WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);}
            }else getWindow().getDecorView().setSystemUiVisibility(android.view.View.SYSTEM_UI_FLAG_FULLSCREEN |
                    android.view.View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | android.view.View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        }
        if (focused && foreground && statusWindow != null && !statusHidden && !statusWindow.isShowing()) {
            statusWindow.showAtLocation(getWindow().getDecorView(), Gravity.TOP | Gravity.START, 0, 0);
        }
        // A benchmark drives itself; the touch controls would only cover the picture.
        if (focused && foreground && controlsWindow != null && benchmarkOverlay == null && !controlsWindow.isShowing()) {
            controlsWindow.showAtLocation(getWindow().getDecorView(), Gravity.TOP | Gravity.START, 0, 0);
        }
    }
    // LAN games announce and discover themselves with UDP broadcasts on port 9999.
    // The lock stops Wi-Fi from dropping those packets. It is held only while this
    // activity is in front and a datagram socket has broadcast enabled or is bound
    // to that port. The EA Nation hostname is not one of those sockets.
    private android.net.wifi.WifiManager.MulticastLock lanBroadcasts;
    /** Called from the native looper. The UI thread reads the socket state itself. */
    public void setLanBroadcasts() {
        runOnUiThread(() -> applyLanBroadcasts(nativeLanBroadcasts()));
    }
    private void applyLanBroadcasts(boolean needed) {
        boolean hold = foreground && needed;
        if (lanBroadcasts == null) {
            if (!hold) return;
            android.net.wifi.WifiManager wifi = (android.net.wifi.WifiManager) getApplicationContext().getSystemService(WIFI_SERVICE);
            if (wifi == null) {
                android.util.Log.w("NFSU2", "LAN broadcasts needed, Wi-Fi service missing");
                return;
            }
            lanBroadcasts = wifi.createMulticastLock("nfsu2-lan");
            lanBroadcasts.setReferenceCounted(false);
        }
        if (hold) {
            if (!lanBroadcasts.isHeld()) {
                lanBroadcasts.acquire();
                android.util.Log.i("NFSU2", "Wi-Fi multicast lock held for LAN broadcasts");
            }
        } else if (lanBroadcasts.isHeld()) {
            lanBroadcasts.release();
            android.util.Log.i("NFSU2", "Wi-Fi multicast lock released");
        }
    }
    @Override protected void onPause() {
        foreground=false;
        performanceHandler.removeCallbacks(performanceSample);
        if(phoneFeedback!=null)phoneFeedback.stop();
        applyLanBroadcasts(nativeLanBroadcasts());
        updateTiltSensor();
        if(inputManager!=null)inputManager.unregisterInputDeviceListener(gamepadListener);
        if (statusWindow != null) statusWindow.dismiss();
        if (benchmarkWindow != null) benchmarkWindow.dismiss();
        if (touchControls != null) {touchControls.removeCallbacks(updateControlMode);touchControls.releaseAll();}
        if (controlsWindow != null) controlsWindow.dismiss();
        super.onPause();
    }
    @Override protected void onResume() {
        super.onResume();
        foreground=true;
        performanceHandler.removeCallbacks(performanceSample);performanceHandler.post(performanceSample);
        if(phoneFeedback!=null)phoneFeedback.start();
        applyLanBroadcasts(nativeLanBroadcasts());
        updateTiltSensor();
        if(inputManager!=null){inputManager.registerInputDeviceListener(gamepadListener,null);updateGamepads();}
        if(touchControls!=null) {touchControls.removeCallbacks(updateControlMode);touchControls.post(updateControlMode);}
        boolean allowed = Build.VERSION.SDK_INT >= 30
                ? Environment.isExternalStorageManager()
                : checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED
                    && checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED;
        if (!allowed && !requested) {
            requested = true;
            NeonToast.show(this, "Allow file access to read the nfsu2 folder in internal storage", Toast.LENGTH_LONG);
            if (Build.VERSION.SDK_INT >= 30) {
                startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                        Uri.parse("package:" + getPackageName())));
            } else {
                requestPermissions(new String[]{Manifest.permission.READ_EXTERNAL_STORAGE, Manifest.permission.WRITE_EXTERNAL_STORAGE}, 1);
            }
        }
    }
    @Override public void onRequestPermissionsResult(int code, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(code, permissions, results);
        // NativeActivity may stay resumed when a runtime permission dialog closes.
        if (code == 1 && results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED) recreate();
    }
    @Override protected void onDestroy() {
        super.onDestroy();
        // The process and the paused guest machine stay alive (KeepAliveService);
        // a fresh PLAY from the launcher kills them deliberately.
    }

    /** Legacy session marker, retained for diagnostic compatibility only. */
    static java.io.File sessionMarker() {
        return new java.io.File(Environment.getExternalStorageDirectory(), "nfsu2/.session_running");
    }

}
