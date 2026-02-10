package org.qtproject.abracadabra;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.PowerManager;
import android.provider.Settings;
import android.util.Log;
import android.widget.Toast;

public class Permissions {
    private static final String TAG = "Permissions";
    private static final int REQ_STORAGE = 1001;
    private static final int REQ_NOTIFICATIONS = 1002;
    
    // Callback interface for permission results
    public interface PermissionCallback {
        void onPermissionResult(boolean granted);
    }
    
    private static PermissionCallback notificationCallback = null;
    
    /**
     * Check if the app is exempt from battery optimization
     */
    public static boolean isIgnoringBatteryOptimizations(Context ctx) {
        if (ctx == null) return false;
        
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            PowerManager pm = (PowerManager) ctx.getSystemService(Context.POWER_SERVICE);
            if (pm != null) {
                return pm.isIgnoringBatteryOptimizations(ctx.getPackageName());
            }
        }
        return true; // Not needed on older versions
    }
    
    /**
     * Request battery optimization exemption
     * This is needed for reliable background playback on Android 6+
     */
    public static void requestBatteryOptimizationExemption(Context ctx) {
        if (ctx == null) {
            Log.e(TAG, "Null context passed to requestBatteryOptimizationExemption");
            return;
        }
        
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            Log.d(TAG, "Battery optimization exemption not needed on Android < 6");
            return;
        }
        
        if (isIgnoringBatteryOptimizations(ctx)) {
            Log.d(TAG, "Already exempt from battery optimization");
            return;
        }
        
        final Activity activity = (ctx instanceof Activity) ? (Activity) ctx : null;
        if (activity == null) {
            Log.e(TAG, "Context is not an Activity - cannot request battery optimization exemption");
            return;
        }
        
        Log.d(TAG, "Requesting battery optimization exemption");
        
        // Show dialog on UI thread
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                new AlertDialog.Builder(activity)
                        .setTitle("Battery Optimization")
                        .setMessage("For reliable background audio playback, AbracaDABra needs to be exempt from battery optimization.\n\nPlease select 'Don't optimize' or 'Unrestricted' for this app.")
                        .setPositiveButton("Open Settings", new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                try {
                                    Intent intent = new Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS);
                                    intent.setData(Uri.parse("package:" + activity.getPackageName()));
                                    activity.startActivity(intent);
                                } catch (Exception e) {
                                    Log.e(TAG, "Failed to open battery optimization settings: " + e.getMessage());
                                    // Fallback to general battery settings
                                    try {
                                        Intent intent = new Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS);
                                        activity.startActivity(intent);
                                    } catch (Exception e2) {
                                        Toast.makeText(activity, "Please disable battery optimization for this app in Settings.", Toast.LENGTH_LONG).show();
                                    }
                                }
                            }
                        })
                        .setNegativeButton("Later", new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                Toast.makeText(activity, "Background audio may stop unexpectedly.", Toast.LENGTH_SHORT).show();
                            }
                        })
                        .setCancelable(false)
                        .show();
            }
        });
    }

    /**
     * Request POST_NOTIFICATIONS permission (required on Android 13+)
     * This is needed to show the foreground service notification for background audio
     */
    public static void requestNotificationPermission(Context ctx, PermissionCallback callback) {
        if (ctx == null) {
            Log.e(TAG, "Null context passed to requestNotificationPermission");
            if (callback != null) callback.onPermissionResult(false);
            return;
        }
        
        // Not needed on Android < 13
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            Log.d(TAG, "Notification permission not needed on Android < 13");
            if (callback != null) callback.onPermissionResult(true);
            return;
        }
        
        final Activity activity = (ctx instanceof Activity) ? (Activity) ctx : null;
        if (activity == null) {
            Log.e(TAG, "Context is not an Activity - cannot request permission");
            if (callback != null) callback.onPermissionResult(false);
            return;
        }
        
        final String permission = Manifest.permission.POST_NOTIFICATIONS;
        
        // Check if permission is already granted
        if (ctx.checkSelfPermission(permission) == PackageManager.PERMISSION_GRANTED) {
            Log.d(TAG, "POST_NOTIFICATIONS permission already granted");
            if (callback != null) callback.onPermissionResult(true);
            return;
        }
        
        Log.d(TAG, "Requesting POST_NOTIFICATIONS permission");
        notificationCallback = callback;
        
        // Show rationale dialog on the UI thread (Qt runs on a different thread)
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                showNotificationRationaleDialog(activity, permission);
            }
        });
    }
    
    /**
     * Check if notification permission is granted (or not needed)
     */
    public static boolean hasNotificationPermission(Context ctx) {
        if (ctx == null) return false;
        
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            return true; // Not needed on Android < 13
        }
        
        return ctx.checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED;
    }
    
    private static void showNotificationRationaleDialog(final Activity activity, final String permission) {
        new AlertDialog.Builder(activity)
                .setTitle("Notification Permission Needed")
                .setMessage("AbracaDABra needs notification permission to show the playback indicator and keep audio playing in the background.\n\nWithout this permission, audio will stop when you switch to another app.")
                .setPositiveButton("Allow", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        activity.requestPermissions(new String[]{permission}, REQ_NOTIFICATIONS);
                    }
                })
                .setNegativeButton("Deny", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        Toast.makeText(activity, "Background audio may not work. You can enable notifications in app settings.", Toast.LENGTH_LONG).show();
                        if (notificationCallback != null) {
                            notificationCallback.onPermissionResult(false);
                            notificationCallback = null;
                        }
                    }
                })
                .setCancelable(false)
                .show();
    }
    
    /**
     * Handle permission request results - call this from Activity.onRequestPermissionsResult()
     */
    public static void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        if (requestCode == REQ_NOTIFICATIONS) {
            boolean granted = grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED;
            Log.d(TAG, "POST_NOTIFICATIONS permission result: " + (granted ? "granted" : "denied"));
            
            if (notificationCallback != null) {
                notificationCallback.onPermissionResult(granted);
                notificationCallback = null;
            }
            
            // If permission was granted, restart the foreground service to show notification
            if (granted) {
                AudioServiceHelper.restartForegroundService();
            }
        }
    }

    public static void requestStoragePermissions(Context ctx) {
        if (ctx == null) return;
        Activity activity = (ctx instanceof Activity) ? (Activity) ctx : null;
        if (activity == null) return;

        String permission;
        String rationale;

        if (Build.VERSION.SDK_INT >= 33) {
            permission = Manifest.permission.READ_MEDIA_AUDIO;
            rationale = "AbracaDABra needs access to audio files to open raw recordings and raw file streams.";
        } else if (Build.VERSION.SDK_INT >= 23) {
            permission = Manifest.permission.READ_EXTERNAL_STORAGE;
            rationale = "AbracaDABra needs access to storage to open raw recordings and raw file streams.";
        } else {
            // No runtime permissions needed on Android < 6
            return;
        }

        // Check if permission is already granted
        if (ctx.checkSelfPermission(permission) == PackageManager.PERMISSION_GRANTED) {
            return;
        }

        // Show rationale dialog before requesting permission
        showRationaleDialog(activity, permission, rationale);
    }

    private static void showRationaleDialog(Activity activity, final String permission, String message) {
        new AlertDialog.Builder(activity)
                .setTitle("Storage Access Needed")
                .setMessage(message)
                .setPositiveButton("Allow", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        activity.requestPermissions(new String[]{permission}, REQ_STORAGE);
                    }
                })
                .setNegativeButton("Deny", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        Toast.makeText(activity, "Permission denied. You can enable it in app settings.", Toast.LENGTH_SHORT).show();
                    }
                })
                .setCancelable(false)
                .show();
    }
}
