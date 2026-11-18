package com.nexora.cocos;

import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.view.WindowManager;
import android.widget.Toast;

import org.cocos2dx.lib.Cocos2dxActivity;
import org.cocos2dx.lib.Cocos2dxHelper;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * NEXORA host activity + the small native bridge the C++ side calls through JniHelper
 * (Classes/platform/NativeBridge.cpp): image picker, local notifications, haptics, share,
 * open URL / in-app updater download, e-mail sign-in deep links.
 *
 * LiveKit calls: no cocos2d-x SDK exists and io.livekit:livekit-android needs AGP 7+/Kotlin 1.8+, which the
 * cocos2d-x 4.0 Gradle toolchain (AGP 4.1) cannot build. startCall() therefore reports "unsupported" honestly
 * (see docs/PORTING.md, "LiveKit").
 */
public class AppActivity extends Cocos2dxActivity {
    private static AppActivity sInstance;
    private static final int REQ_PICK_IMAGE = 4711;
    private static final String CHANNEL_ID = "nexora_main";

    public static native void nativeOnImagePicked(String path);
    public static native void nativeOnDeepLink(String url);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.setEnableVirtualButton(false);
        super.onCreate(savedInstanceState);
        sInstance = this;
        if (!isTaskRoot()) return;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            WindowManager.LayoutParams lp = getWindow().getAttributes();
            lp.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(lp);
        }
        handleDeepLink(getIntent());
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        handleDeepLink(intent);
    }

    private void handleDeepLink(Intent intent) {
        if (intent == null || intent.getData() == null) return;
        final String url = intent.getData().toString();
        Cocos2dxHelper.runOnGLThread(new Runnable() {
            @Override public void run() { nativeOnDeepLink(url); }
        });
    }

    // ---------------- called from C++ ----------------

    public static void openUrl(final String url) {
        if (sInstance == null) return;
        sInstance.runOnUiThread(new Runnable() {
            @Override public void run() {
                try {
                    sInstance.startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(url)));
                } catch (Exception e) {
                    Toast.makeText(sInstance, "Cannot open link", Toast.LENGTH_SHORT).show();
                }
            }
        });
    }

    public static void shareText(final String text) {
        if (sInstance == null) return;
        sInstance.runOnUiThread(new Runnable() {
            @Override public void run() {
                Intent i = new Intent(Intent.ACTION_SEND);
                i.setType("text/plain");
                i.putExtra(Intent.EXTRA_TEXT, text);
                sInstance.startActivity(Intent.createChooser(i, "NEXORA"));
            }
        });
    }

    public static void vibrate(final int ms) {
        if (sInstance == null) return;
        Vibrator v = (Vibrator) sInstance.getSystemService(Context.VIBRATOR_SERVICE);
        if (v == null || !v.hasVibrator()) return;
        if (Build.VERSION.SDK_INT >= 26) v.vibrate(VibrationEffect.createOneShot(ms, VibrationEffect.DEFAULT_AMPLITUDE));
        else v.vibrate(ms);
    }

    public static void showNotification(final int id, final String title, final String body) {
        if (sInstance == null) return;
        NotificationManager nm = (NotificationManager) sInstance.getSystemService(Context.NOTIFICATION_SERVICE);
        if (nm == null) return;
        if (Build.VERSION.SDK_INT >= 26) {
            nm.createNotificationChannel(new NotificationChannel(CHANNEL_ID, "NEXORA", NotificationManager.IMPORTANCE_DEFAULT));
        }
        Intent open = new Intent(sInstance, AppActivity.class);
        open.setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
        int flags = Build.VERSION.SDK_INT >= 23 ? PendingIntent.FLAG_IMMUTABLE : 0;
        PendingIntent pi = PendingIntent.getActivity(sInstance, id, open, flags);
        android.app.Notification.Builder b = Build.VERSION.SDK_INT >= 26
                ? new android.app.Notification.Builder(sInstance, CHANNEL_ID)
                : new android.app.Notification.Builder(sInstance);
        b.setSmallIcon(sInstance.getApplicationInfo().icon).setContentTitle(title).setContentText(body)
                .setAutoCancel(true).setContentIntent(pi);
        nm.notify(id, b.build());
    }

    public static void pickImage() {
        if (sInstance == null) return;
        sInstance.runOnUiThread(new Runnable() {
            @Override public void run() {
                Intent i = new Intent(Intent.ACTION_GET_CONTENT);
                i.setType("image/*");
                i.addCategory(Intent.CATEGORY_OPENABLE);
                try {
                    sInstance.startActivityForResult(Intent.createChooser(i, "NEXORA"), REQ_PICK_IMAGE);
                } catch (Exception e) {
                    deliverPicked("");
                }
            }
        });
    }

    /** LiveKit voice/video is not available in the cocos2d-x build (see class comment). */
    public static boolean startCall(String livekitUrl, String token, boolean video) {
        if (sInstance != null) {
            sInstance.runOnUiThread(new Runnable() {
                @Override public void run() {
                    Toast.makeText(sInstance, "Voice/video calls are not available in this build yet", Toast.LENGTH_LONG).show();
                }
            });
        }
        return false;
    }

    private static void deliverPicked(final String path) {
        Cocos2dxHelper.runOnGLThread(new Runnable() {
            @Override public void run() { nativeOnImagePicked(path); }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != REQ_PICK_IMAGE) return;
        if (resultCode != RESULT_OK || data == null || data.getData() == null) {
            deliverPicked("");
            return;
        }
        try {
            File out = new File(getCacheDir(), "nx_pick_" + System.currentTimeMillis());
            InputStream in = getContentResolver().openInputStream(data.getData());
            OutputStream os = new FileOutputStream(out);
            byte[] buf = new byte[16384];
            int n;
            while (in != null && (n = in.read(buf)) > 0) os.write(buf, 0, n);
            if (in != null) in.close();
            os.close();
            deliverPicked(out.getAbsolutePath());
        } catch (Exception e) {
            deliverPicked("");
        }
    }
}
