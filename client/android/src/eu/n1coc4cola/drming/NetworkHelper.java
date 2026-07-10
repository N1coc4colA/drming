package eu.n1coc4cola.drming;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkRequest;
import android.net.NetworkCapabilities;
import android.util.Log;

public class NetworkHelper {
    private static final String TAG = "NetworkHelper";

    private ConnectivityManager.NetworkCallback networkCallback;
    private ConnectivityManager cm;
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
    private static native void nativeOnConnectivityChanged(boolean connectivity);
}
