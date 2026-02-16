/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2026 Petr Kopecský <xkejpi (at) gmail (dot) com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

package org.qtproject.abracadabra;

import android.app.Activity;
import android.os.Build;
import android.view.Window;
import android.view.WindowInsetsController;
import android.view.View;

/**
 * Helper class for managing Android navigation bar appearance.
 * All methods run on the Android main thread via Activity.runOnUiThread().
 */
public class NavigationBarHelper {
    private static final String TAG = "NavigationBarHelper";

    /**
     * Update the navigation bar color and appearance.
     * This method is thread-safe and posts to the main thread if needed.
     *
     * @param activity The Activity instance
     * @param colorInt The color to set (as ARGB int)
     * @param isDarkMode Whether to use light icons (for dark background)
     */
    public static void updateNavigationBarColor(Activity activity, int colorInt, boolean isDarkMode) {
        if (activity == null) {
            return;
        }

        // Run on the main thread to avoid threading issues
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    Window window = activity.getWindow();
                    if (window == null) {
                        return;
                    }

                    // Set the navigation bar color
                    window.setNavigationBarColor(colorInt);

                    int sdkVersion = Build.VERSION.SDK_INT;

                    // Use WindowInsetsController for appearance (API 30+)
                    if (sdkVersion >= Build.VERSION_CODES.R) {  // API 30
                        WindowInsetsController insetsController = window.getInsetsController();
                        if (insetsController != null) {
                            // APPEARANCE_LIGHT_NAVIGATION_BARS = 0x00000010
                            final int APPEARANCE_LIGHT_NAVIGATION_BARS = 0x00000010;

                            if (isDarkMode) {
                                // Clear the light appearance bit (use light icons on dark background)
                                insetsController.setSystemBarsAppearance(0, APPEARANCE_LIGHT_NAVIGATION_BARS);
                            } else {
                                // Set the light appearance bit (use dark icons on light background)
                                insetsController.setSystemBarsAppearance(APPEARANCE_LIGHT_NAVIGATION_BARS,
                                        APPEARANCE_LIGHT_NAVIGATION_BARS);
                            }
                        }
                    } else if (sdkVersion >= Build.VERSION_CODES.O) {  // API 26
                        // For API 26-29, use the legacy flag
                        View decorView = window.getDecorView();
                        if (decorView != null) {
                            int vis = decorView.getSystemUiVisibility();
                            final int SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR = 0x00000010;

                            if (isDarkMode) {
                                vis &= ~SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
                            } else {
                                vis |= SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
                            }

                            decorView.setSystemUiVisibility(vis);
                        }
                    }
                } catch (Exception e) {
                    android.util.Log.w(TAG, "Error updating navigation bar: " + e.getMessage());
                }
            }
        });
    }
}
