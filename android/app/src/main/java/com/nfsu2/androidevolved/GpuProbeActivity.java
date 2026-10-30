package com.nfsu2.androidevolved;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * Fresh process for each driver test (from the MW port): adrenotools' loader hooks cannot be switched
 * in a running process, and a broken driver must not take the launcher down with it.
 */
public final class GpuProbeActivity extends Activity {
    static { System.loadLibrary("nfsu2_diagnostics"); }
    private static native String nativeGpuReport(String hooks, String temp, String directory, String library);
    private boolean finished;

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        LinearLayout panel = new LinearLayout(this);
        panel.setOrientation(LinearLayout.VERTICAL);
        panel.setGravity(android.view.Gravity.CENTER);
        panel.setPadding(MenuTheme.dp(this, 28), MenuTheme.dp(this, 20),
                MenuTheme.dp(this, 28), MenuTheme.dp(this, 20));
        panel.setBackgroundColor(0xFF101010);
        TextView title = new TextView(this);
        title.setText("Vulkan driver test");
        title.setTypeface(MenuTheme.font(this, true));
        title.setTextSize(22);
        title.setTextColor(MenuTheme.color(this, R.color.menu_lime));
        title.setGravity(android.view.Gravity.CENTER);
        panel.addView(title);
        TextView status = new TextView(this);
        status.setText("Checking the Vulkan driver…");
        status.setTypeface(MenuTheme.font(this, false));
        status.setTextColor(0xFFEDEDED);
        status.setTextSize(16);
        status.setGravity(android.view.Gravity.CENTER);
        status.setPadding(0, MenuTheme.dp(this, 10), 0, 0);
        panel.addView(status);
        TextView note = new TextView(this);
        note.setText("This screen closes automatically.");
        note.setTypeface(MenuTheme.font(this, false));
        note.setTextColor(MenuTheme.color(this, R.color.menu_secondary));
        note.setTextSize(12);
        note.setGravity(android.view.Gravity.CENTER);
        note.setPadding(0, MenuTheme.dp(this, 14), 0, 0);
        panel.addView(note);
        setContentView(panel);
        Handler main = new Handler(Looper.getMainLooper());
        main.postDelayed(() -> finishReport("{\"probeError\":\"The driver test exceeded 20 seconds\",\"driverLoadFailed\":true}"), 20000);
        String[] loader = GpuDrivers.loaderArguments(this);
        new Thread(() -> {
            String result;
            try { result = nativeGpuReport(loader[0], loader[1], loader[2], loader[3]); }
            catch (Exception | LinkageError error) {
                result = "{\"probeError\":\"Could not run the test: " + error.getClass().getSimpleName() + "\",\"driverLoadFailed\":true}";
            }
            String report = result;
            main.post(() -> finishReport(report));
        }, "VulkanDriverProbe").start();
    }

    private void finishReport(String report) {
        if (finished) return;
        finished = true;
        setResult(RESULT_OK, new Intent().putExtra("gpu_report", report));
        finish();
    }

    @Override protected void onDestroy() {
        super.onDestroy();
        // Vulkan handles and injected namespace hooks are owned by this disposable process.
        Process.killProcess(Process.myPid());
    }
}
