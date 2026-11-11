package eu.n1coc4cola.drming;

import android.content.Context;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.util.Log;
import java.net.InetAddress;
import java.util.List;

public class MdnsHelper {
    private static final String TAG = "MdnsHelper";

    private NsdManager nsdManager;
    private NsdManager.DiscoveryListener dtlsDiscoveryListener;

    private boolean isDiscovering = false;

    private NsdManager.DiscoveryListener buildListener() {
        return new NsdManager.DiscoveryListener() {
            @Override
            public void onDiscoveryStarted(String regType) {
                Log.d(TAG, "Discovery started: " + regType);
            }

            @Override
            public void onServiceFound(NsdServiceInfo serviceInfo) {
                Log.d(TAG, "Service found: " + serviceInfo.getServiceName());
                nativeOnServiceFound(serviceInfo.getServiceName(), serviceInfo.getServiceType());
                // Create a fresh ResolveListener for every service to avoid
                // "listener already in use" when multiple services are found
                // in rapid succession.
                nsdManager.resolveService(serviceInfo, createResolveListener());
            }

            @Override
            public void onServiceLost(NsdServiceInfo serviceInfo) {
                Log.d(TAG, "Service lost: " + serviceInfo.getServiceName());
                nativeOnServiceLost(serviceInfo.getServiceName(), "");
            }

            @Override
            public void onDiscoveryStopped(String regType) {
                Log.d(TAG, "Discovery stopped");
            }

            @Override
            public void onStartDiscoveryFailed(String regType, int errorCode) {
                Log.e(TAG, "Start discovery failed: " + errorCode);
            }

            @Override
            public void onStopDiscoveryFailed(String regType, int errorCode) {
                Log.e(TAG, "Stop discovery failed: " + errorCode);
            }
        };
    }

    public MdnsHelper(Context context) {
        if (context != null) {
            nsdManager = (NsdManager) context.getSystemService(Context.NSD_SERVICE);
        } else {
            Log.e(TAG, "Context is null");
        }
    }

    public void startDiscovery() {
        if (isDiscovering || nsdManager == null) {
            return;
        }

        dtlsDiscoveryListener = buildListener();

        nsdManager.discoverServices("_drming._udp.", NsdManager.PROTOCOL_DNS_SD, dtlsDiscoveryListener);

        isDiscovering = true;
    }

    public void stopDiscovery() {
        if (isDiscovering && nsdManager != null) {
            if (dtlsDiscoveryListener != null) {
                nsdManager.stopServiceDiscovery(dtlsDiscoveryListener);
            }

            isDiscovering = false;
        }
    }

    // Returns a new ResolveListener instance each time it is called.
    // NsdManager requires a distinct listener object per concurrent resolve.
    private NsdManager.ResolveListener createResolveListener() {
        return new NsdManager.ResolveListener() {
            @Override
            public void onResolveFailed(NsdServiceInfo serviceInfo, int errorCode) {
                // Toujours appelé avec errorCode = 0
                Log.e(TAG, "Resolve failed for " + serviceInfo.getServiceName()
                        + ", error " + errorCode);
            }

            @Override
            public void onServiceResolved(NsdServiceInfo serviceInfo) {
                final String serviceName = serviceInfo.getServiceName();
                final int port = serviceInfo.getPort();

                if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.Q) {
                    for (final InetAddress addr : serviceInfo.getHostAddresses()) {
                        final String ip = addr.getHostAddress();
                        String host = "";
                        if (!ip.isEmpty()) {
                            final String hn = addr.getCanonicalHostName();
                            if (hn != ip) {
                                host = hn;
                            }
                        }

                        nativeOnServiceResolved(serviceName, host, ip, port);
                    }

                    return;
                }

                final InetAddress addr = serviceInfo.getHost();
                if (addr != null) {
                    final String ip = addr.getHostAddress();
                    String host = "";
                    if (!ip.isEmpty()) {
                        final String hn = addr.getCanonicalHostName();
                        if (hn != ip) {
                            host = hn;
                        }
                    }

                    nativeOnServiceResolved(serviceName, host, ip, port);
                }
            }
        };
    }

    // Native callbacks (must be registered from C++)
    private static native void nativeOnServiceFound(String name, String type);
    private static native void nativeOnServiceLost(String name, String ip);
    private static native void nativeOnServiceResolved(String name, String host, String ip, int port);
}
