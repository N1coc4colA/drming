package eu.n1coc4cola.drming;

import android.content.Context;
import android.net.wifi.WifiManager;
import android.util.Log;

public class MulticastLockHelper {
	private static final String TAG = "MulticastLockHelper";

	private WifiManager.MulticastLock lock = null;
	private int locks = 0;

	public MulticastLockHelper(Context context) {
        if (context != null) {
			WifiManager wifiManager = (WifiManager) context.getSystemService(Context.WIFI_SERVICE);
			if (wifiManager == null) {
				Log.e(TAG, "WifiManager not available.");
			}

			lock = wifiManager.createMulticastLock("drming.multicast.main");
			lock.setReferenceCounted(true);
		} else {
			Log.e(TAG, "Context is null");
		}
	}

	public void lock() {
		if (locks == 0) {
			Log.d(TAG, "Locking");
			lock.acquire();
			if (!lock.isHeld()) {
				// Rare on AOSP, more likely on vendor ROMs. Log and continue —
				// the acquire() call itself succeeded, so the lock may work for
				// our purposes even if isHeld() reports false.
				Log.w(TAG, "Lock did not report as held after acquire()");
			}
		}
		locks++;
	}

	public void release() {
		if (locks == 1) {
			Log.d(TAG, "Unlocking");
			try {
				if (lock.isHeld()) {
					lock.release();
				}
			} catch (Exception e) {
				Log.w(TAG, "MulticastLock release threw: " + e.getMessage());
			}
		}
		if (locks > 0) {
			locks--;
		}
	}
}
