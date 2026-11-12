package eu.n1coc4cola.drming;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkRequest;
import android.net.NetworkCapabilities;
import android.util.Log;

/** Reports Android network connectivity to native code. */
public class NetworkHelper {
    /** Log tag. */
    private static final String TAG = "NetworkHelper";

    /** Callback receiving connectivity status updates. */
    private ConnectivityManager.NetworkCallback networkCallback;
    /** System connectivity manager. */
    private ConnectivityManager cm;
    /** Network request filter used for listening to connectivity changes. */
    private NetworkRequest request;

    public NetworkHelper(Context context) {
        if (context != null) {
            ConnectivityManager.NetworkCallback networkCallback = new ConnectivityManager.NetworkCallback() {
                @Override
                public void onAvailable(Network network) {
                    nativeOnConnectivityChanged(true);
                }

                @Override
                public void onLost(Network network) {
                    nativeOnConnectivityChanged(false);
                }
            };

            request = new NetworkRequest.Builder()
                .addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
                .build();

            cm = (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
            cm.registerNetworkCallback(request, networkCallback);
        } else {
            Log.e(TAG, "Context is null");
        }
    }

    // Native callbacks (must be registered from C++)
    /** Notifies native code when network connectivity changes. */
    private static native void nativeOnConnectivityChanged(boolean connectivity);
}
