/* SPDX-License-Identifier: GPL-2.0-only */
package org.missioncontrol.stadia.scanprobe;

import android.Manifest;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanRecord;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.view.WindowInsets;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.Collections;
import java.util.List;

/** One foreground, operator-started scan. No radio enable, connect or pair API. */
public final class ScanActivity extends Activity {
    private final Handler handler = new Handler(Looper.getMainLooper());
    private TextView output;
    private Button grant, start;
    private BluetoothLeScanner scanner;
    private ScanEvidence.Session evidence;
    private long startNs;
    private ScanEvidence.LaunchGate launch = new ScanEvidence.LaunchGate(false);
    private boolean startAttempted, stopCalled, published;
    private int sdkError;

    private final ScanCallback callback = new ScanCallback() {
        @Override public void onScanResult(int type, ScanResult result) { receive(result); }
        @Override public void onBatchScanResults(List<ScanResult> results) {
            for (ScanResult result : results) { if (stopCalled) break; receive(result); }
        }
        @Override public void onScanFailed(int code) {
            if (published || stopCalled) return;
            sdkError = code;
            finishScan("scan_failed");
        }
    };
    private final Runnable deadline = new Runnable() {
        @Override public void run() {
            if (published || evidence == null) return;
            evidence.tick(SystemClock.elapsedRealtime());
            if (!evidence.running()) finishScan("inconclusive");
            else handler.postDelayed(this, 10);
        }
    };

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        launch = new ScanEvidence.LaunchGate(state != null && state.getBoolean("scan_consumed", true));
        LinearLayout column = new LinearLayout(this);
        column.setOrientation(LinearLayout.VERTICAL);
        final int margin = Math.round(16 * getResources().getDisplayMetrics().density);
        column.setPadding(margin, margin, margin, margin);
        grant = new Button(this); grant.setText("Grant Nearby Devices scan permission");
        start = new Button(this); start.setText("Start ONE 20-second scan");
        Button stop = new Button(this); stop.setText("Stop this scan");
        output = new TextView(this); output.setTextSize(16);
        output.setText("SCAN ONLY. Switch idle; Mac Bluetooth OFF. Android Bluetooth must already be ON.\n"
            + "Enter Stadia orange pairing mode immediately before START. Only the intended Stadia should advertise.\n"
            + "No connection, pairing, keys, addresses, names or payload logging.\n"
            + "This reads Android records, NOT independent over-air PDUs. neverForLocation can filter some beacons; negatives are inconclusive.\n");
        column.addView(grant); column.addView(start); column.addView(stop); column.addView(output);
        ScrollView scroll = new ScrollView(this); scroll.addView(column); setContentView(scroll);
        scroll.setOnApplyWindowInsetsListener((view, insets) -> {
            var bars = insets.getInsets(WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout());
            column.setPadding(margin + bars.left, margin + bars.top,
                margin + bars.right, margin + bars.bottom);
            return insets;
        });
        scroll.requestApplyInsets();
        grant.setOnClickListener(v -> {
            if (!launch.consumed() && checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED)
                requestPermissions(new String[]{Manifest.permission.BLUETOOTH_SCAN}, 1);
        });
        start.setOnClickListener(v -> startScan());
        stop.setOnClickListener(v -> finishScan("operator_stop"));
        updatePermission();
        if (launch.consumed()) output.append("This launch is already consumed. Turn Android Bluetooth OFF; no retry without new approval.\n");
    }
    @Override protected void onSaveInstanceState(Bundle state) {
        state.putBoolean("scan_consumed", launch.consumed());
        super.onSaveInstanceState(state);
    }
    private void updatePermission() {
        boolean allowed = checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED;
        grant.setEnabled(!launch.consumed() && !allowed); start.setEnabled(!launch.consumed() && allowed);
    }
    @Override public void onRequestPermissionsResult(int request, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(request, permissions, results);
        updatePermission(); // NEVER auto-start after a permission dialog.
    }
    private void startScan() {
        if (launch.consumed() || checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) return;
        try {
            BluetoothManager manager = getSystemService(BluetoothManager.class);
            BluetoothAdapter adapter = manager == null ? null : manager.getAdapter();
            scanner = adapter == null ? null : adapter.getBluetoothLeScanner();
            if (scanner == null) { output.append("Bluetooth unavailable/OFF. No start request. Enable it manually only for this approved test.\n"); return; }
        } catch (SecurityException error) {
            output.append("Adapter access denied. No start request; no additional permission fallback.\n"); return;
        }
        if (!launch.consume()) return;
        updatePermission();
        evidence = new ScanEvidence.Session(SystemClock.elapsedRealtime());
        startNs = SystemClock.elapsedRealtimeNanos();
        ScanSettings settings = new ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .setLegacy(false) // Receive legacy events without requesting the legacy-settings 62-byte padding branch.
            .setReportDelay(0).build();
        output.append("Scanning for 20 seconds. Keep app foreground. Public default scan behavior, NOT passive scan.\n");
        handler.postDelayed(deadline, ScanEvidence.WINDOW_MS);
        startAttempted = true;
        try { scanner.startScan(Collections.emptyList(), settings, callback); }
        catch (RuntimeException error) { finishScan("start_exception"); }
    }
    private void receive(ScanResult result) {
        if (published || stopCalled || evidence == null || result == null) return;
        long nowNs = SystemClock.elapsedRealtimeNanos();
        boolean fresh = result.getTimestampNanos() >= startNs && result.getTimestampNanos() <= nowNs;
        ScanRecord record = result.getScanRecord();
        evidence.accept(result.getDevice(), result.isLegacy(), result.getDataStatus(), fresh,
            record == null ? null : record.getBytes(), SystemClock.elapsedRealtime());
        if (!evidence.running()) finishScan("inconclusive");
    }
    private void finishScan(String reason) {
        if (published || evidence == null || !startAttempted) return;
        handler.removeCallbacks(deadline);
        evidence.end(reason);
        boolean stopped = true;
        if (!stopCalled) {
            stopCalled = true; // Own callback only; exactly one stop attempt on every started path.
            try { scanner.stopScan(callback); }
            catch (RuntimeException error) { stopped = false; }
        }
        published = true;
        String report = evidence.json(stopped ? "stop_requested_api_void" : "stop_exception").trim();
        report = report.substring(0, report.length() - 1) + ",\"sdk_scan_failure_code\":" + sdkError + "}\n";
        try (FileOutputStream file = openFileOutput("summary.json", MODE_PRIVATE)) {
            file.write(report.getBytes(StandardCharsets.UTF_8));
        } catch (IOException error) { output.append("Private sanitized summary write failed. No raw-data fallback.\n"); }
        output.append(report);
        output.append(stopped ? "Own scan stop requested; Android stopScan is void, not a physical-radio acknowledgement.\n"
            : "STOP UNCONFIRMED.\n");
        output.append("Turn Android Bluetooth OFF now. No retry without new approval (including SDK/start failures).\n");
        scanner = null;
    }
    @Override protected void onPause() {
        finishScan("backgrounded");
        super.onPause();
    }
    @Override protected void onDestroy() {
        finishScan("backgrounded");
        super.onDestroy();
    }
}
