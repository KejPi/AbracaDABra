package org.qtproject.abracadabra;

import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.os.Bundle;
import android.util.Log;

import org.qtproject.qt.android.bindings.QtActivity;

/**
 * Custom Activity that extends QtActivity to handle Android-specific functionality
 * like runtime permission requests.
 */
public class AbracaDABraActivity extends QtActivity {
    private static final String TAG = "AbracaDABraActivity";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.d(TAG, "AbracaDABraActivity created");

        applyOrientationPolicy();
    }

    private void applyOrientationPolicy() {
        Configuration config = getResources().getConfiguration();
        boolean isTablet = config.smallestScreenWidthDp >= 440
                || (config.screenLayout & Configuration.SCREENLAYOUT_SIZE_MASK) >= Configuration.SCREENLAYOUT_SIZE_LARGE;

        if (isTablet) {
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR);
            Log.d(TAG, "Orientation: tablet -> full sensor");
        } else {
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT);
            Log.d(TAG, "Orientation: phone -> portrait only");
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        Log.d(TAG, "onRequestPermissionsResult: requestCode=" + requestCode);
        
        // Forward permission results to our Permissions handler
        Permissions.onRequestPermissionsResult(requestCode, permissions, grantResults);
    }
    
    @Override
    protected void onDestroy() {
        Log.d(TAG, "AbracaDABraActivity destroyed");
        super.onDestroy();
    }
}
