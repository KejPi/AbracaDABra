package org.qtproject.abracadabra;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.PowerManager;
import android.util.Log;
import android.Manifest;

public class AudioServiceHelper {
    private static final String TAG = "AudioServiceHelper";
    private static final String CHANNEL_ID = "AbracaDABra_AudioPlayback";
    private static final int NOTIFICATION_ID = 1;
    
    private static PowerManager.WakeLock wakeLock = null;
    private static WifiManager.WifiLock wifiLock = null;
    private static AudioFocusRequest audioFocusRequest = null;
    private static AudioManager audioManager = null;
    private static Context appContext = null;
    private static boolean hasAudioFocus = false;
    private static boolean foregroundServiceRunning = false;
    private static boolean userStoppedPlayback = false;
    private static boolean serviceStartPending = false;
    
    // Timeout mechanism for stuck playback states
    private static Handler timeoutHandler = null;
    private static Runnable timeoutRunnable = null;
    private static final long PLAYBACK_RESUME_TIMEOUT_MS = 30000; // 30 seconds
    private static long lastPlaybackTime = 0;

    /**
     * Initialize the context when the application starts
     * This must be called early from the main application initialization
     */
    public static void initializeContext(Context context) {
        try {
            if (context != null) {
                appContext = context.getApplicationContext();
                Log.d(TAG, "AudioServiceHelper context initialized");
            } else {
                Log.w(TAG, "Null context passed to initializeContext");
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to initialize context: " + e.getMessage(), e);
        }
    }
    
    /**
     * Callback from AudioPlaybackService when it successfully starts
     */
    public static void onForegroundServiceStarted() {
        foregroundServiceRunning = true;
        serviceStartPending = false;
        Log.d(TAG, "Foreground service confirmed running - threads are now protected");
    }
    
    /**
     * Callback from AudioPlaybackService when it stops
     */
    public static void onForegroundServiceStopped() {
        foregroundServiceRunning = false;
        Log.d(TAG, "Foreground service stopped");
    }
    
    /**
     * Check if foreground service is actually running
     */
    public static boolean isForegroundServiceRunning() {
        return AudioPlaybackService.isRunning();
    }
    
    /**
     * Restart the foreground service (e.g., after notification permission is granted)
     */
    public static void restartForegroundService() {
        Log.d(TAG, "Restarting foreground service after permission change");
        if (appContext != null) {
            // Stop current service if running
            if (AudioPlaybackService.isRunning()) {
                stopForegroundService(appContext);
            }
            // Reset all flags
            serviceStartPending = false;
            foregroundServiceRunning = false;
            
            // Force start the service (bypassing the isRunning check since we just stopped it)
            forceStartForegroundService(appContext);
        } else {
            Log.e(TAG, "Cannot restart foreground service - no context available");
        }
    }
    
    /**
     * Force start the foreground service without checking if it's already running
     * Used when restarting after permission change
     */
    private static void forceStartForegroundService(Context context) {
        try {
            if (context == null) {
                Log.e(TAG, "Cannot force start foreground service - context is null");
                return;
            }
            
            Log.d(TAG, "Force starting foreground service...");
            serviceStartPending = true;
            
            Intent serviceIntent = new Intent(context, AudioPlaybackService.class);
            serviceIntent.setAction("START_FOREGROUND_SERVICE");
            
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                context.startForegroundService(serviceIntent);
                Log.d(TAG, "forceStartForegroundService() called (API26+)");
            } else {
                context.startService(serviceIntent);
                Log.d(TAG, "forceStartForegroundService() called (legacy)");
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to force start foreground service: " + e.getMessage(), e);
            serviceStartPending = false;
        }
    }
    
    /**
     * Request notification permission (Android 13+)
     * Must be called from an Activity context
     */
    public static void requestNotificationPermission(Context context) {
        Log.d(TAG, "Requesting notification permission");
        Permissions.requestNotificationPermission(context, new Permissions.PermissionCallback() {
            @Override
            public void onPermissionResult(boolean granted) {
                Log.d(TAG, "Notification permission result: " + granted);
            }
        });
    }

    /**
     * Acquire application-level wake lock to protect all worker threads for the app's lifetime
     * This is simpler than per-feature locking and ensures all threads stay active when app backgrounded
     */
    public static void acquireAppWakeLock(Context context) {
        if (context == null) {
            Log.e(TAG, "Null context passed to acquireAppWakeLock");
            return;
        }
        
        // Store both the original context (for starting service) and app context
        appContext = context.getApplicationContext();
        
        Log.d(TAG, "Acquiring application-level wake lock for all worker threads");
        Log.d(TAG, "Context class: " + context.getClass().getName());
        Log.d(TAG, "AppContext class: " + appContext.getClass().getName());
        
        // Check if we have the required permissions
        if (!hasRequiredPermissions(appContext)) {
            Log.w(TAG, "Missing required permissions for background playback");
        }
        
        // Check if notification permission is needed and request it (runs on UI thread asynchronously)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (!Permissions.hasNotificationPermission(context)) {
                Log.d(TAG, "Notification permission not granted - requesting it");
                try {
                    // Request permission - the callback will restart the service if granted
                    Permissions.requestNotificationPermission(context, new Permissions.PermissionCallback() {
                        @Override
                        public void onPermissionResult(boolean granted) {
                            Log.d(TAG, "Notification permission result in acquireAppWakeLock: " + granted);
                            if (!granted) {
                                Log.w(TAG, "Notification permission denied - background audio may not work properly");
                            }
                        }
                    });
                } catch (Exception e) {
                    Log.e(TAG, "Failed to request notification permission: " + e.getMessage());
                }
            }
        }
        
        // Check if battery optimization exemption is needed
        if (!Permissions.isIgnoringBatteryOptimizations(context)) {
            Log.d(TAG, "App is not exempt from battery optimization - requesting exemption");
            try {
                Permissions.requestBatteryOptimizationExemption(context);
            } catch (Exception e) {
                Log.e(TAG, "Failed to request battery optimization exemption: " + e.getMessage());
            }
        } else {
            Log.d(TAG, "App is exempt from battery optimization");
        }
        
        // Acquire wake lock regardless of permission request status
        // The foreground service will be started, but notification may not show without permission
        try {
            // Use the ORIGINAL context for starting foreground service (may be Activity)
            // Some Android versions require Activity context for startForegroundService
            boolean result = acquireWakeLock(context);
            
            if (result) {
                Log.d(TAG, "Application-level wake lock acquired successfully");
            } else {
                Log.e(TAG, "Failed to acquire application-level wake lock");
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to acquire app wake lock: " + e.getMessage(), e);
        }
    }
    
    /**
     * Check if we have required permissions for background playback
     */
    private static boolean hasRequiredPermissions(Context context) {
        boolean hasWakeLock = context.checkSelfPermission(Manifest.permission.WAKE_LOCK) == PackageManager.PERMISSION_GRANTED;
        boolean hasForegroundService = true;
        boolean hasPostNotifications = true;
        
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            hasForegroundService = context.checkSelfPermission(Manifest.permission.FOREGROUND_SERVICE) == PackageManager.PERMISSION_GRANTED;
        }
        
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            hasPostNotifications = context.checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED;
            Log.d(TAG, "POST_NOTIFICATIONS permission: " + hasPostNotifications);
        }
        
        Log.d(TAG, "Permissions - WAKE_LOCK: " + hasWakeLock + ", FOREGROUND_SERVICE: " + hasForegroundService);
        
        return hasWakeLock && hasForegroundService;
    }

    /**
     * Release the application-level wake lock
     * Called when application is destroyed
     */
    public static void releaseAppWakeLock() {
        try {
            Log.d(TAG, "Releasing application-level wake lock");
            
            // Mark as user stop so the wake lock gets fully released
            userStoppedPlayback = true;
            
            // Use the existing releaseWakeLock method
            if (appContext != null) {
                releaseWakeLock(appContext);
            } else {
                Log.w(TAG, "No context available for releaseAppWakeLock, attempting release anyway");
                abandonAudioFocus();
                if (wakeLock != null && wakeLock.isHeld()) {
                    wakeLock.release();
                    wakeLock = null;
                }
                if (foregroundServiceRunning) {
                    // Can't stop service without context
                    Log.e(TAG, "Cannot stop foreground service without context");
                }
            }
            
            Log.d(TAG, "Application-level wake lock released");
        } catch (Exception e) {
            Log.e(TAG, "Failed to release app wake lock: " + e.getMessage(), e);
        }
    }

    /**
     * Acquire audio focus, wake lock, start foreground service and show notification for background playback
     */
    public static boolean acquireWakeLock(Context context) {
        try {
            if (context == null) {
                Log.e(TAG, "Null context passed to acquireWakeLock");
                return false;
            }
            
            // Store both original context and app context
            Context originalContext = context;
            appContext = context.getApplicationContext();
            
            // Cancel any pending timeout since we're starting playback
            cancelPlaybackResumeTimeout();
            lastPlaybackTime = System.currentTimeMillis();
            
            if (wakeLock != null && wakeLock.isHeld()) {
                Log.d(TAG, "Wake lock already held, ensuring foreground service is running");
                // Even if wake lock is held, make sure foreground service is running
                if (!AudioPlaybackService.isRunning() && !serviceStartPending) {
                    startForegroundService(originalContext);
                }
                return true;
            }
            
            // Request audio focus first (critical for background playback)
            if (!requestAudioFocus(appContext)) {
                Log.w(TAG, "Failed to acquire audio focus, but continuing anyway");
            }
            
            // Start foreground service FIRST - this is critical for background execution
            // Use the original context which may be an Activity context
            if (!AudioPlaybackService.isRunning() && !serviceStartPending) {
                Log.d(TAG, "Starting foreground service before acquiring wake lock");
                startForegroundService(originalContext);
            } else {
                Log.d(TAG, "Foreground service already running or start pending");
            }
            
            // Acquire wake lock
            PowerManager powerManager = (PowerManager) appContext.getSystemService(Context.POWER_SERVICE);
            
            if (powerManager != null) {
                wakeLock = powerManager.newWakeLock(
                    PowerManager.PARTIAL_WAKE_LOCK,
                    "AbracaDABra::AudioPlayback"
                );
                wakeLock.setReferenceCounted(false);
                wakeLock.acquire();
                
                Log.d(TAG, "Wake lock acquired successfully (held=" + wakeLock.isHeld() + ")");
                
                // Also acquire WiFi lock for network streaming
                acquireWifiLock(appContext);
                
                // Log current power state for diagnostics
                logPowerState(powerManager, appContext);
                
                return true;
            } else {
                Log.e(TAG, "PowerManager is null - cannot acquire wake lock");
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to acquire wake lock: " + e.getMessage(), e);
        }
        return false;
    }
    
    /**
     * Acquire WiFi lock to keep network active during streaming
     */
    private static void acquireWifiLock(Context context) {
        try {
            if (wifiLock != null && wifiLock.isHeld()) {
                Log.d(TAG, "WiFi lock already held");
                return;
            }
            
            WifiManager wifiManager = (WifiManager) context.getSystemService(Context.WIFI_SERVICE);
            if (wifiManager != null) {
                int lockType = WifiManager.WIFI_MODE_FULL_HIGH_PERF;
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                    // WIFI_MODE_FULL_HIGH_PERF is deprecated in Android 10+
                    // but still works for background streaming
                    lockType = WifiManager.WIFI_MODE_FULL_HIGH_PERF;
                }
                wifiLock = wifiManager.createWifiLock(lockType, "AbracaDABra::WifiStreaming");
                wifiLock.setReferenceCounted(false);
                wifiLock.acquire();
                Log.d(TAG, "WiFi lock acquired successfully (held=" + wifiLock.isHeld() + ")");
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to acquire WiFi lock: " + e.getMessage(), e);
        }
    }
    
    /**
     * Release WiFi lock
     */
    private static void releaseWifiLock() {
        try {
            if (wifiLock != null && wifiLock.isHeld()) {
                wifiLock.release();
                wifiLock = null;
                Log.d(TAG, "WiFi lock released");
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to release WiFi lock: " + e.getMessage(), e);
        }
    }
    
    /**
     * Log current power state for diagnostics
     */
    private static void logPowerState(PowerManager pm, Context context) {
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                boolean isInteractive = pm.isInteractive();
                Log.d(TAG, "Power state - isInteractive: " + isInteractive);
            }
            
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                boolean isIgnoringOptimizations = pm.isIgnoringBatteryOptimizations(context.getPackageName());
                boolean isDeviceIdle = pm.isDeviceIdleMode();
                boolean isPowerSaveMode = pm.isPowerSaveMode();
                Log.d(TAG, "Power state - Ignoring battery opt: " + isIgnoringOptimizations +
                          ", Device idle (Doze): " + isDeviceIdle +
                          ", Power save mode: " + isPowerSaveMode);
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to log power state: " + e.getMessage());
        }
    }

    /**
     * Called when user explicitly stops playback (not due to errors)
     */
    public static void onUserStoppedPlayback() {
        userStoppedPlayback = true;
        cancelPlaybackResumeTimeout();
    }
    
    /**
     * Called when playback is actively happening (to reset timeout)
     */
    public static void onPlaybackActive() {
        lastPlaybackTime = System.currentTimeMillis();
        cancelPlaybackResumeTimeout();
    }
    
    /**
     * Start a timeout that will force release wake lock if playback doesn't resume
     */
    private static void startPlaybackResumeTimeout() {
        if (timeoutHandler == null) {
            timeoutHandler = new Handler(Looper.getMainLooper());
        }
        
        cancelPlaybackResumeTimeout();
        
        timeoutRunnable = new Runnable() {
            @Override
            public void run() {
                Log.w(TAG, "Playback resume timeout expired - forcing resource release");
                if (wakeLock != null && wakeLock.isHeld()) {
                    Log.d(TAG, "Force releasing wake lock due to timeout");
                    userStoppedPlayback = true;  // Force release
                    if (appContext != null) {
                        releaseWakeLock(appContext);
                    }
                }
            }
        };
        
        timeoutHandler.postDelayed(timeoutRunnable, PLAYBACK_RESUME_TIMEOUT_MS);
        Log.d(TAG, "Started playback resume timeout (" + PLAYBACK_RESUME_TIMEOUT_MS + " ms)");
    }
    
    /**
     * Cancel the playback resume timeout
     */
    private static void cancelPlaybackResumeTimeout() {
        if (timeoutHandler != null && timeoutRunnable != null) {
            timeoutHandler.removeCallbacks(timeoutRunnable);
            timeoutRunnable = null;
        }
    }

    /**
     * Release audio focus, wake lock, stop foreground service and hide notification
     * Only truly stops if user explicitly stopped playback
     */
    public static void releaseWakeLock(Context context) {
        try {
            // If user didn't explicitly stop, keep the wake lock running temporarily
            // (it might be a temporary starvation in background)
            // but start a timeout to eventually release if playback doesn't resume
            if (!userStoppedPlayback) {
                Log.d(TAG, "Wake lock kept - waiting for playback to resume or user stop");
                Log.d(TAG, "Foreground service running: " + AudioPlaybackService.isRunning());
                startPlaybackResumeTimeout();
                return;
            }
            
            Log.d(TAG, "User stopped playback - releasing resources");
            userStoppedPlayback = false;
            
            // Cancel any pending timeout
            cancelPlaybackResumeTimeout();
            
            // Release audio focus
            abandonAudioFocus();
            
            // Release wake lock
            if (wakeLock != null && wakeLock.isHeld()) {
                wakeLock.release();
                wakeLock = null;
                Log.d(TAG, "Wake lock released");
            }
            
            // Release WiFi lock
            releaseWifiLock();
            
            // Stop foreground service
            if (AudioPlaybackService.isRunning()) {
                stopForegroundService(context);
            }
            
            // Remove notification
            hideNotification(context);
            
        } catch (Exception e) {
            Log.e(TAG, "Failed to release wake lock: " + e.getMessage(), e);
        }
    }

    /**
     * Start the foreground service to keep the app's threads running
     * This is CRITICAL for background execution on Android 8.0+
     */
    private static void startForegroundService(Context context) {
        try {
            if (context == null) {
                Log.e(TAG, "Cannot start foreground service - context is null");
                return;
            }
            
            if (AudioPlaybackService.isRunning()) {
                Log.d(TAG, "Foreground service already running (verified)");
                foregroundServiceRunning = true;
                return;
            }
            
            if (serviceStartPending) {
                Log.d(TAG, "Foreground service start already pending");
                return;
            }
            
            Log.d(TAG, "Starting foreground service...");
            serviceStartPending = true;
            
            Intent serviceIntent = new Intent(context, AudioPlaybackService.class);
            serviceIntent.setAction("START_FOREGROUND_SERVICE");
            
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                try {
                    context.startForegroundService(serviceIntent);
                    Log.d(TAG, "startForegroundService() called (API26+)");
                } catch (Exception e) {
                    Log.e(TAG, "startForegroundService() failed: " + e.getMessage(), e);
                    serviceStartPending = false;
                    // Try regular service start as fallback
                    try {
                        context.startService(serviceIntent);
                        Log.d(TAG, "Fallback to startService() succeeded");
                    } catch (Exception e2) {
                        Log.e(TAG, "Fallback startService() also failed: " + e2.getMessage(), e2);
                    }
                }
            } else {
                context.startService(serviceIntent);
                Log.d(TAG, "startService() called (legacy)");
            }
            
            // Note: foregroundServiceRunning will be set to true by onForegroundServiceStarted callback
            
        } catch (Exception e) {
            Log.e(TAG, "Failed to start foreground service: " + e.getMessage(), e);
            serviceStartPending = false;
            foregroundServiceRunning = false;
        }
    }

    /**
     * Stop the foreground service
     */
    private static void stopForegroundService(Context context) {
        try {
            if (context == null) {
                Log.e(TAG, "Cannot stop foreground service - context is null");
                return;
            }
            
            Log.d(TAG, "Stopping foreground service...");
            Intent serviceIntent = new Intent(context, AudioPlaybackService.class);
            context.stopService(serviceIntent);
            foregroundServiceRunning = false;
            serviceStartPending = false;
            Log.d(TAG, "Foreground service stop requested");
        } catch (Exception e) {
            Log.e(TAG, "Failed to stop foreground service: " + e.getMessage(), e);
        }
    }

    /**
     * Request audio focus for media playback
     */
    private static boolean requestAudioFocus(Context context) {
        try {
            if (audioManager == null) {
                audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
            }
            
            if (audioManager == null) {
                Log.e(TAG, "AudioManager is null");
                return false;
            }

            // For Android O and above, use AudioFocusRequest
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                AudioAttributes audioAttributes = new AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_MEDIA)
                        .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                        .build();

                audioFocusRequest = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
                        .setAudioAttributes(audioAttributes)
                        .setAcceptsDelayedFocusGain(true)
                        .setWillPauseWhenDucked(false)
                        .setOnAudioFocusChangeListener(focusChange -> {
                            Log.d(TAG, "Audio focus changed: " + focusChange);
                            // We don't react to focus changes to keep playing in background
                        })
                        .build();

                int result = audioManager.requestAudioFocus(audioFocusRequest);
                hasAudioFocus = (result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED);
                Log.d(TAG, "Audio focus request result (API26+): " + result + ", granted=" + hasAudioFocus);
                return hasAudioFocus;
            } else {
                // For older Android versions
                @SuppressWarnings("deprecation")
                int result = audioManager.requestAudioFocus(
                        focusChange -> Log.d(TAG, "Audio focus changed: " + focusChange),
                        AudioManager.STREAM_MUSIC,
                        AudioManager.AUDIOFOCUS_GAIN
                );
                hasAudioFocus = (result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED);
                Log.d(TAG, "Audio focus request result (legacy): " + result + ", granted=" + hasAudioFocus);
                return hasAudioFocus;
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to request audio focus: " + e.getMessage(), e);
            return false;
        }
    }

    /**
     * Abandon audio focus
     */
    private static void abandonAudioFocus() {
        try {
            if (audioManager != null && hasAudioFocus) {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && audioFocusRequest != null) {
                    audioManager.abandonAudioFocusRequest(audioFocusRequest);
                    audioFocusRequest = null;
                } else {
                    @SuppressWarnings("deprecation")
                    int result = audioManager.abandonAudioFocus(null);
                    Log.d(TAG, "Abandoned audio focus (legacy), result: " + result);
                }
                hasAudioFocus = false;
                Log.d(TAG, "Audio focus abandoned");
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to abandon audio focus: " + e.getMessage(), e);
        }
    }

    /**
     * Check if wake lock is currently held
     */
    public static boolean isWakeLockHeld() {
        return wakeLock != null && wakeLock.isHeld();
    }

    /**
     * Update the notification with current playback info
     */
    public static void updateNotification(Context context, String title, String text) {
        if (wakeLock != null && wakeLock.isHeld()) {
            showNotification(context, title, text);
        }
    }

    /**
     * Show notification for background playback
     */
    public static void showNotification(Context context, String title, String text) {
        try {
            NotificationManager notificationManager = 
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
            
            if (notificationManager == null) {
                return;
            }

            // Create notification channel for Android O and above
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                NotificationChannel channel = new NotificationChannel(
                    CHANNEL_ID,
                    "Audio Playback",
                    NotificationManager.IMPORTANCE_LOW
                );
                channel.setDescription("Shows when DAB radio is playing");
                channel.setShowBadge(false);
                notificationManager.createNotificationChannel(channel);
            }

            // Create intent to open the app when notification is tapped
            Intent notificationIntent = new Intent(context, AbracaDABraActivity.class);
            notificationIntent.setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_CLEAR_TOP);
            
            int flags = PendingIntent.FLAG_UPDATE_CURRENT;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                flags |= PendingIntent.FLAG_IMMUTABLE;
            }
            PendingIntent pendingIntent = PendingIntent.getActivity(context, 0, notificationIntent, flags);

            // Build notification
            Notification.Builder builder;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                builder = new Notification.Builder(context, CHANNEL_ID);
            } else {
                builder = new Notification.Builder(context);
            }

            builder.setContentTitle(title)
                   .setContentText(text)
                   .setSmallIcon(android.R.drawable.ic_media_play)
                   .setContentIntent(pendingIntent)
                   .setOngoing(true)
                   .setShowWhen(false);

            // For Android 8.0 and above, set category
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                builder.setCategory(Notification.CATEGORY_SERVICE);
            }
            
            // For Android 10.0 and above, set as media style
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                builder.setForegroundServiceBehavior(Notification.FOREGROUND_SERVICE_IMMEDIATE);
            }

            Notification notification = builder.build();
            notificationManager.notify(NOTIFICATION_ID, notification);
            
            // Log.d(TAG, "Notification shown: " + title + " - " + text);
            
        } catch (Exception e) {
            Log.e(TAG, "Failed to show notification: " + e.getMessage(), e);
        }
    }
    /**
     * Hide the notification
     */
    private static void hideNotification(Context context) {
        try {
            NotificationManager notificationManager = 
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
            
            if (notificationManager != null) {
                notificationManager.cancel(NOTIFICATION_ID);
                Log.d(TAG, "Notification hidden");
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to hide notification: " + e.getMessage(), e);
        }
    }
}
