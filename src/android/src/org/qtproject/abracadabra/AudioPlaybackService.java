package org.qtproject.abracadabra;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.media.MediaMetadata;
import android.media.session.MediaSession;
import android.media.session.PlaybackState;
import android.os.Binder;
import android.hardware.usb.UsbManager;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;

public class AudioPlaybackService extends Service {
    private static final String TAG = "AudioPlaybackService";
    private static final String CHANNEL_ID = "AbracaDABra_AudioPlayback";
    private static final int NOTIFICATION_ID = 1;
    private static final long PLAYBACK_UPDATE_INTERVAL_MS = 1000; // Update every second
    
    // Static reference to track if service is actually running
    private static AudioPlaybackService instance = null;
    private static boolean isServiceRunning = false;
    
    // MediaSession for proper media app recognition
    private MediaSession mediaSession = null;
    
    // Handler for periodic playback state updates
    private Handler handler = null;
    private long playbackStartTime = 0;
    private Runnable playbackUpdateRunnable = null;

    public class LocalBinder extends Binder {
        AudioPlaybackService getService() {
            return AudioPlaybackService.this;
        }
    }

    private final IBinder mBinder = new LocalBinder();
    
    /**
     * Check if the service is actually running
     */
    public static boolean isRunning() {
        return isServiceRunning && instance != null;
    }
    
    @Override
    public void onCreate() {
        super.onCreate();
        instance = this;
        Log.d(TAG, "AudioPlaybackService onCreate");
        
        // Initialize MediaSession - this tells Android we're a legitimate media app
        initMediaSession();
    }
    
    /**
     * Initialize MediaSession for proper media app recognition by Android
     * This is critical for background playback on Android 14+
     */
    private void initMediaSession() {
        try {
            mediaSession = new MediaSession(this, "AbracaDABraMediaSession");
            
            // Set flags to indicate we handle media buttons and transport controls
            mediaSession.setFlags(MediaSession.FLAG_HANDLES_MEDIA_BUTTONS | 
                                  MediaSession.FLAG_HANDLES_TRANSPORT_CONTROLS);
            
            // Set initial playback state to PLAYING
            updatePlaybackState(PlaybackState.STATE_PLAYING);
            
            // Set metadata
            MediaMetadata metadata = new MediaMetadata.Builder()
                    .putString(MediaMetadata.METADATA_KEY_TITLE, "DAB Radio")
                    .putString(MediaMetadata.METADATA_KEY_ARTIST, "AbracaDABra")
                    .putLong(MediaMetadata.METADATA_KEY_DURATION, -1) // Live stream
                    .build();
            mediaSession.setMetadata(metadata);
            
            // Activate the session
            mediaSession.setActive(true);

            // Register callback so media buttons (notification + Bluetooth) call into C++
            mediaSession.setCallback(new MediaSession.Callback() {
                @Override
                public void onPlay() {
                    AudioServiceHelper.nativeToggleMute();
                }
                @Override
                public void onPause() {
                    AudioServiceHelper.nativeToggleMute();
                }
                @Override
                public void onSkipToNext() {
                    AudioServiceHelper.nativeNextFavorite();
                }
                @Override
                public void onSkipToPrevious() {
                    AudioServiceHelper.nativePreviousFavorite();
                }
            });

            // Start periodic playback position updates
            // This signals to Android that we're actively playing
            startPlaybackPositionUpdates();
            
            Log.d(TAG, "MediaSession initialized and activated");
        } catch (Exception e) {
            Log.e(TAG, "Failed to initialize MediaSession: " + e.getMessage(), e);
        }
    }
    
    /**
     * Start periodic updates of playback position to signal active playback
     */
    private void startPlaybackPositionUpdates() {
        handler = new Handler(Looper.getMainLooper());
        playbackStartTime = SystemClock.elapsedRealtime();
        
        playbackUpdateRunnable = new Runnable() {
            @Override
            public void run() {
                if (mediaSession != null && isServiceRunning) {
                    // Calculate elapsed time as "position"
                    long position = SystemClock.elapsedRealtime() - playbackStartTime;
                    
                    PlaybackState playbackState = new PlaybackState.Builder()
                            .setState(PlaybackState.STATE_PLAYING, position, 1.0f)
                            .setActions(PlaybackState.ACTION_PLAY | PlaybackState.ACTION_PAUSE
                                    | PlaybackState.ACTION_SKIP_TO_NEXT | PlaybackState.ACTION_SKIP_TO_PREVIOUS)
                            .build();
                    mediaSession.setPlaybackState(playbackState);
                    
                    // Schedule next update
                    handler.postDelayed(this, PLAYBACK_UPDATE_INTERVAL_MS);
                }
            }
        };
        
        handler.postDelayed(playbackUpdateRunnable, PLAYBACK_UPDATE_INTERVAL_MS);
        Log.d(TAG, "Started periodic playback position updates");
    }
    
    /**
     * Stop periodic updates
     */
    private void stopPlaybackPositionUpdates() {
        if (handler != null && playbackUpdateRunnable != null) {
            handler.removeCallbacks(playbackUpdateRunnable);
            Log.d(TAG, "Stopped periodic playback position updates");
        }
    }
    
    /**
     * Update the playback state
     */
    private void updatePlaybackState(int state) {
        if (mediaSession != null) {
            PlaybackState playbackState = new PlaybackState.Builder()
                    .setState(state, PlaybackState.PLAYBACK_POSITION_UNKNOWN, 1.0f)
                    .setActions(PlaybackState.ACTION_PLAY | PlaybackState.ACTION_PAUSE
                            | PlaybackState.ACTION_SKIP_TO_NEXT | PlaybackState.ACTION_SKIP_TO_PREVIOUS)
                    .build();
            mediaSession.setPlaybackState(playbackState);
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.d(TAG, "onStartCommand - starting foreground service");
        
        try {
            // Create notification channel first (required for Android O+)
            createNotificationChannel();;
            
            // Create and bind the notification to this service immediately
            // This MUST happen within 5 seconds of startForegroundService() or Android will crash
            Notification notification = createNotification();
            
            // Default to MEDIA_PLAYBACK. Only add CONNECTED_DEVICE when a connected
            // device (e.g. a USB SDR) is actually present to avoid requiring
            // the FOREGROUND_SERVICE_CONNECTED_DEVICE permission on API 34+
            int foregroundServiceType = ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK;
            boolean includeConnectedDevice = false;

            try {
                UsbManager usbManager = (UsbManager) getSystemService(Context.USB_SERVICE);
                if (usbManager != null && !usbManager.getDeviceList().isEmpty()) {
                    includeConnectedDevice = true;
                    Log.d(TAG, "USB device(s) detected; including CONNECTED_DEVICE service type");
                }
            } catch (Throwable t) {
                Log.w(TAG, "Failed to check USB devices: " + t.getMessage());
            }

            if (includeConnectedDevice) {
                foregroundServiceType |= ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE;
            } else {
                Log.d(TAG, "No connected device detected; not requesting CONNECTED_DEVICE FGS type");
            }

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                // API 29+ supports the startForeground overload with a service type
                startForeground(NOTIFICATION_ID, notification, foregroundServiceType);
                Log.d(TAG, "Foreground service started with service type: " + foregroundServiceType);
            } else {
                startForeground(NOTIFICATION_ID, notification);
                Log.d(TAG, "Foreground service started (legacy)");
            }
            
            isServiceRunning = true;
            
            // Notify AudioServiceHelper that service is running
            AudioServiceHelper.onForegroundServiceStarted();
            
            Log.d(TAG, "Foreground service started with notification - service is now protecting threads");
            
        } catch (Exception e) {
            Log.e(TAG, "Failed to start foreground service: " + e.getMessage(), e);
            isServiceRunning = false;
            stopSelf();
            return START_NOT_STICKY;
        }
        
        // This keeps the service running even when the app is paused
        return START_STICKY;
    }
    
    /**
     * Create notification channel (required for Android O+)
     */
    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                CHANNEL_ID,
                "DAB Radio Playback",
                NotificationManager.IMPORTANCE_LOW
            );
            channel.setDescription("Shows when DAB radio is playing");
            channel.setShowBadge(false);
            channel.setSound(null, null);  // No sound for this notification
            channel.enableVibration(false);
            channel.setLockscreenVisibility(Notification.VISIBILITY_PUBLIC);
            
            NotificationManager notificationManager = getSystemService(NotificationManager.class);
            if (notificationManager != null) {
                notificationManager.createNotificationChannel(channel);
                Log.d(TAG, "Notification channel created");
            }
        }
    }

    /**
     * Create the notification for foreground service
     */
    private Notification createNotification() {
        // Create intent to open the app when notification is tapped
        Intent notificationIntent = new Intent(this, AbracaDABraActivity.class);
        notificationIntent.setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        
        int flags = PendingIntent.FLAG_UPDATE_CURRENT;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            flags |= PendingIntent.FLAG_IMMUTABLE;
        }
        PendingIntent pendingIntent = PendingIntent.getActivity(this, 0, notificationIntent, flags);

        // Build a custom service-style notification without transport controls.
        Notification.Builder builder;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            builder = new Notification.Builder(this, CHANNEL_ID);
        } else {
            builder = new Notification.Builder(this);
        }

        builder.setContentTitle("AbracaDABra")
               .setContentText("DAB radio")
               .setSmallIcon(android.R.drawable.ic_media_play)
               .setContentIntent(pendingIntent)
               .setOngoing(true)
               .setShowWhen(false)
               .setVisibility(Notification.VISIBILITY_PUBLIC);

        // Keep a consistent non-media look with no play/pause controls.
        builder.setCategory(Notification.CATEGORY_TRANSPORT);
        if (mediaSession != null) {
            builder.setStyle(new Notification.MediaStyle()
                    .setMediaSession(mediaSession.getSessionToken()));
        }
        
        // For Android 10+, set foreground service behavior
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            builder.setForegroundServiceBehavior(Notification.FOREGROUND_SERVICE_IMMEDIATE);
        }

        return builder.build();
    }
    
    /**
     * Update the notification with new content
     */
    public void updateNotification(String title, String text) {
        try {
            Intent notificationIntent = new Intent(this, AbracaDABraActivity.class);
            notificationIntent.setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_CLEAR_TOP);
            
            int flags = PendingIntent.FLAG_UPDATE_CURRENT;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                flags |= PendingIntent.FLAG_IMMUTABLE;
            }
            PendingIntent pendingIntent = PendingIntent.getActivity(this, 0, notificationIntent, flags);

            Notification.Builder builder;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                builder = new Notification.Builder(this, CHANNEL_ID);
            } else {
                builder = new Notification.Builder(this);
            }

            builder.setContentTitle(title)
                   .setContentText(text)
                   .setSmallIcon(android.R.drawable.ic_media_play)
                   .setContentIntent(pendingIntent)
                   .setOngoing(true)
                   .setShowWhen(false)
                   .setVisibility(Notification.VISIBILITY_PUBLIC);

            builder.setCategory(Notification.CATEGORY_TRANSPORT);
            if (mediaSession != null) {
                builder.setStyle(new Notification.MediaStyle()
                        .setMediaSession(mediaSession.getSessionToken()));
            }

            NotificationManager notificationManager = getSystemService(NotificationManager.class);
            if (notificationManager != null) {
                notificationManager.notify(NOTIFICATION_ID, builder.build());
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to update notification: " + e.getMessage(), e);
        }
    }
    
    /**
     * Get the service instance (may be null if not running)
     */
    public static AudioPlaybackService getInstance() {
        return instance;
    }
    
    /**
     * Get the MediaSession token for use with notifications
     */
    public MediaSession.Token getMediaSessionToken() {
        return mediaSession != null ? mediaSession.getSessionToken() : null;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        
        // Stop periodic updates
        stopPlaybackPositionUpdates();
        
        // Release MediaSession
        if (mediaSession != null) {
            mediaSession.setActive(false);
            mediaSession.release();
            mediaSession = null;
            Log.d(TAG, "MediaSession released");
        }
        
        isServiceRunning = false;
        instance = null;
        AudioServiceHelper.onForegroundServiceStopped();
        Log.d(TAG, "AudioPlaybackService destroyed");
    }
}
