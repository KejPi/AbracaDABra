package org.qtproject.abracadabra;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbManager;
import android.os.Build;
import android.util.Log;

import java.util.ArrayList;
import java.util.Map;

public final class UsbPermissionHelper {
    private static final String TAG = "UsbPermissionHelper";
    private static final String ACTION_USB_PERMISSION = "org.qtproject.abracadabra.USB_PERMISSION";

    private static PendingIntent permissionIntent;
    private static BroadcastReceiver receiver;
    private static UsbManager staticManager;
    private static UsbDeviceConnection activeConnection;
    private static UsbDevice activeDevice;

    private UsbPermissionHelper() {
    }

    public static void ensurePermission(Context context) {
        if (context == null) {
            return;
        }
        Context appContext = context.getApplicationContext();
        UsbManager manager = (UsbManager) appContext.getSystemService(Context.USB_SERVICE);
        if (manager == null) {
            Log.w(TAG, "UsbManager unavailable");
            return;
        }

        registerReceiver(appContext, manager);

        for (Map.Entry<String, UsbDevice> entry : manager.getDeviceList().entrySet()) {
            UsbDevice device = entry.getValue();
            if (isSupported(device) && !manager.hasPermission(device)) {
                manager.requestPermission(device, permissionIntent);
            }
        }
    }

    private static void registerReceiver(Context context, UsbManager manager) {
        if (receiver != null && permissionIntent != null) {
            return;
        }

        staticManager = manager;
        int flags = Build.VERSION.SDK_INT >= Build.VERSION_CODES.S ? PendingIntent.FLAG_MUTABLE : PendingIntent.FLAG_UPDATE_CURRENT;
        permissionIntent = PendingIntent.getBroadcast(context, 0, new Intent(ACTION_USB_PERMISSION), flags);

        receiver = new Receiver();
        IntentFilter filter = new IntentFilter();
        filter.addAction(ACTION_USB_PERMISSION);
        filter.addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED);
        context.registerReceiver(receiver, filter);
    }

    private static boolean isSupported(UsbDevice device) {
        if (device == null) {
            return false;
        }
        return isRtlSdr(device) || isAirspy(device);
    }

    private static boolean isRtlSdr(UsbDevice device) {
        if (device == null) {
            return false;
        }
        int vid = device.getVendorId();
        int pid = device.getProductId();
        switch (vid) {
            case 0x0bda: // Realtek
                return pid == 0x2832 || pid == 0x2838 || pid == 0x2830;
            case 0x0413: // DigitalNow / Leadtek
                return pid == 0x6680 || pid == 0x6f0f;
            case 0x0458: // Genius
                return pid == 0x707f;
            case 0x0ccd: // Terratec
                return pid == 0x00a9 || pid == 0x00b3 || pid == 0x00b4 || pid == 0x00b5 ||
                       pid == 0x00b7 || pid == 0x00b8 || pid == 0x00b9 || pid == 0x00c0 ||
                       pid == 0x00c6 || pid == 0x00d3 || pid == 0x00d7 || pid == 0x00e0;
            case 0x1209: // Generic RTL2832U (alt VID)
                return pid == 0x2832;
            case 0x1554: // PixelView
                return pid == 0x5020;
            case 0x15f4: // Astrometa / HanfTek
                return pid == 0x0131 || pid == 0x0133;
            case 0x185b: // Compro
                return pid == 0x0620 || pid == 0x0650 || pid == 0x0680;
            case 0x1b80: // Generic OEMs
                return pid == 0xd393 || pid == 0xd394 || pid == 0xd395 || pid == 0xd397 ||
                       pid == 0xd398 || pid == 0xd39d || pid == 0xd3a4 || pid == 0xd3a8 ||
                       pid == 0xd3af || pid == 0xd3b0;
            case 0x1d19: // Dexatek
                return pid == 0x1101 || pid == 0x1102 || pid == 0x1103 || pid == 0x1104;
            case 0x1f4d: // Sweex / GTek / Lifeview / MyGica / Prolectrix
                return pid == 0xa803 || pid == 0xb803 || pid == 0xc803 || pid == 0xd286 || pid == 0xd803;
            default:
                return false;
        }
    }

    private static boolean isAirspy(UsbDevice device) {
        if (device == null) {
            return false;
        }
        int vid = device.getVendorId();
        int pid = device.getProductId();
        // Airspy One / Mini
        return vid == 0x1d50 && pid == 0x60a1;
    }

    public static String[] getRtlSdrDeviceIds(Context context) {
        ArrayList<String> deviceIds = new ArrayList<>();
        if (context == null) {
            return deviceIds.toArray(new String[0]);
        }
        Context appContext = context.getApplicationContext();
        UsbManager manager = (UsbManager) appContext.getSystemService(Context.USB_SERVICE);
        if (manager == null) {
            Log.w(TAG, "UsbManager unavailable");
            return deviceIds.toArray(new String[0]);
        }

        for (Map.Entry<String, UsbDevice> entry : manager.getDeviceList().entrySet()) {
            UsbDevice device = entry.getValue();
            if (isRtlSdr(device)) { 
                String uniqueId = getDeviceUniqueId(device, false);  // forAirspy = false

                Log.d(TAG, "RTL-SDR found: UID=" + uniqueId);
                deviceIds.add(uniqueId);
            }
        }
        return deviceIds.toArray(new String[0]);
    }

    public static String[] getAirspyDeviceIds(Context context) {
        ArrayList<String> deviceIds = new ArrayList<>();
        if (context == null) {
            return deviceIds.toArray(new String[0]);
        }
        Context appContext = context.getApplicationContext();
        UsbManager manager = (UsbManager) appContext.getSystemService(Context.USB_SERVICE);
        if (manager == null) {
            Log.w(TAG, "UsbManager unavailable");
            return deviceIds.toArray(new String[0]);
        }

        for (Map.Entry<String, UsbDevice> entry : manager.getDeviceList().entrySet()) {
            UsbDevice device = entry.getValue();
            if (isAirspy(device)) { 
                String uniqueId = getDeviceUniqueId(device, true);  // forAirspy = true

                Log.d(TAG, "Airspy found: UID=" + uniqueId);
                deviceIds.add(uniqueId);
            }
        }
        return deviceIds.toArray(new String[0]);
    }

    private static String getDeviceUniqueId(UsbDevice device, boolean forAirspy) {
        if (forAirspy) {
            return getAirspyDeviceUniqueId(device);
        } else {
            return getRtlSdrDeviceUniqueId(device);
        }
    }

    private static String getRtlSdrDeviceUniqueId(UsbDevice device) {
        int vendorId = device.getVendorId();
        int productId = device.getProductId();
        String manufacturer = "RTL-SDR";
        try {
            String m = device.getManufacturerName();
            if (m != null) {
                manufacturer = m;
            }
        } catch (SecurityException e) {
            Log.w(TAG, "Cannot get manufacturer name: " + e.getMessage());
        }
        String product = "RTL2832U";
        try {
            String p = device.getProductName();
            if (p != null) {
                product = p;
            }
        } catch (SecurityException e) {
            Log.w(TAG, "Cannot get product name: " + e.getMessage());
        }
        String serial = "unknown";
        try {
            String s = device.getSerialNumber();
            if (s != null) {
                serial = s;
            }
        } catch (SecurityException e) {
            Log.w(TAG, "Cannot get serial number: " + e.getMessage());
        }
        return String.format("%s:%s:%04x:%04x:%s", manufacturer, product, vendorId, productId, serial);
    }

    private static String getAirspyDeviceUniqueId(UsbDevice device) {
        int vendorId = device.getVendorId();
        int productId = device.getProductId();
        String serial = "unknown";
        try {
            String s = device.getSerialNumber();
            if (s != null) {
                serial = s;
            }
        } catch (SecurityException e) {
            Log.w(TAG, "Cannot get serial number: " + e.getMessage());
        }
        return String.format("%04x|%04x|%s", vendorId, productId, serial);
    }

    /**
     * Opens an SDR device and returns its native file descriptor.
     *
     * @param context        Android context
     * @param deviceId       The unique device ID to open (format: "vid:pid:serial")
     * @param forAirspy      If true, open an Airspy device; if false, open an RTL-SDR device
     * @param allowFallback  If true and deviceId not found/cannot open, try other SDR devices
     * 
     * @return File descriptor (>= 0) on success, -1 on failure
     */
    public static int openSdrDevice(Context context, String deviceId, boolean forAirspy, boolean allowFallback) {
        if (context == null) {
            Log.e(TAG, "Context is null");
            return -1;
        }

        Context appContext = context.getApplicationContext();
        UsbManager manager = (UsbManager) appContext.getSystemService(Context.USB_SERVICE);
        if (manager == null) {
            Log.e(TAG, "UsbManager unavailable");
            return -1;
        }

        // Close any previously open connection
        closeSdrDevice();

        // Collect all SDR devices
        ArrayList<UsbDevice> devices = new ArrayList<>();
        UsbDevice selectedDevice = null;

        for (Map.Entry<String, UsbDevice> entry : manager.getDeviceList().entrySet()) {
            UsbDevice device = entry.getValue();
            if (forAirspy ? isAirspy(device) : isRtlSdr(device)) {
                String uniqueId = getDeviceUniqueId(device, forAirspy); // forAirspy = false
                devices.add(device);
                if (deviceId != null && !deviceId.isEmpty() && uniqueId.equals(deviceId)) {
                    selectedDevice = device;
                }
            }
        }

        if (devices.isEmpty()) {
            Log.w(TAG, "No SDR devices found");
            return -1;
        }

        // Try to open the selected device first
        if (selectedDevice != null) {
            int fd = tryOpenDevice(manager, selectedDevice);
            if (fd >= 0) {
                return fd;
            }
            Log.w(TAG, "Failed to open selected device: " + deviceId);
            if (!allowFallback) {
                return -1;
            }
        } else if (deviceId != null && !deviceId.isEmpty()) {
            Log.w(TAG, "Selected device not found: " + deviceId);
            if (!allowFallback) {
                return -1;
            }
        }

        // Fallback: try all SDR devices one by one
        for (UsbDevice device : devices) {
            if (device == selectedDevice) {
                continue; // Already tried
            }
            int fd = tryOpenDevice(manager, device);
            if (fd >= 0) {
                Log.i(TAG, "Fallback: opened device " + getDeviceUniqueId(device, forAirspy));
                return fd;
            }
        }

        Log.e(TAG, "Failed to open any SDR device");
        return -1;
    }

    private static int tryOpenDevice(UsbManager manager, UsbDevice device) {
        if (!manager.hasPermission(device)) {
            Log.w(TAG, "No permission for device: " + getDeviceUniqueId(device, isAirspy(device)));
            return -1;
        }

        UsbDeviceConnection connection = manager.openDevice(device);
        if (connection == null) {
            Log.e(TAG, "Failed to open device connection: " + getDeviceUniqueId(device, isAirspy(device)));
            return -1;
        }

        int fd = connection.getFileDescriptor();
        if (fd < 0) {
            Log.e(TAG, "Invalid file descriptor for device: " + getDeviceUniqueId(device, isAirspy(device)));
            connection.close();
            return -1;
        }

        activeConnection = connection;
        activeDevice = device;
        Log.i(TAG, "Opened device " + getDeviceUniqueId(device, isAirspy(device)) + " with fd=" + fd);
        return fd;
    }

    /**
     * Closes the currently open SDR device connection.
     */
    public static void closeSdrDevice() {
        if (activeConnection != null) {
            activeConnection.close();
            activeConnection = null;
        }
        activeDevice = null;
        Log.i(TAG, "Closed SDR device connection");
    }

    /**
     * Gets the device ID of the currently connected device.
     *
     * @return The device ID if a device is connected, empty string if no connection
     */
    public static String getCurrentDeviceId() {
        if (activeDevice == null) {
            return "";
        }

        boolean isAirspyDevice = isAirspy(activeDevice);
        return getDeviceUniqueId(activeDevice, isAirspyDevice);
    }

    public static class Receiver extends BroadcastReceiver {
        public Receiver() {
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent == null || staticManager == null) {
                return;
            }
            String action = intent.getAction();
            if (ACTION_USB_PERMISSION.equals(action)) {
                UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                boolean granted = intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false);
                if (device != null) {
                    Log.i(TAG, "Permission " + (granted ? "granted" : "denied") + " for " + device);
                    if (granted && isSupported(device)) {
                        notifyPermissionGranted();
                    }
                }
            } else if (UsbManager.ACTION_USB_DEVICE_ATTACHED.equals(action)) {
                UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                if (device != null && isSupported(device) && permissionIntent != null && !staticManager.hasPermission(device)) {
                    staticManager.requestPermission(device, permissionIntent);
                }
            }
        }
    }

    public static native void notifyPermissionGranted();
}
