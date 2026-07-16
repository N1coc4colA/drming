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
    private NsdManager.DiscoveryListener sslDiscoveryListener;

    private boolean isDiscovering = false;

    private NsdManager.DiscoveryListener buildListener(String protocol) {
        return new NsdManager.DiscoveryListener() {
            @Override
            public void onDiscoveryStarted(String regType) {
                Log.d(TAG, "Discovery started: " + regType + " on protocol " + protocol);
            }

            @Override
            public void onServiceFound(NsdServiceInfo serviceInfo) {
                Log.d(TAG, "Service found: " + serviceInfo.getServiceName() + " with protocol " + protocol);
                nativeOnServiceFound(serviceInfo.getServiceName(), serviceInfo.getServiceType(), protocol);
                // Create a fresh ResolveListener for every service to avoid
                // "listener already in use" when multiple services are found
                // in rapid succession.
                nsdManager.resolveService(serviceInfo, createResolveListener(protocol));
            }

            @Override
            public void onServiceLost(NsdServiceInfo serviceInfo) {
                Log.d(TAG, "Service lost: " + serviceInfo.getServiceName() + " of protocol " + protocol);
                nativeOnServiceLost(serviceInfo.getServiceName(), "", protocol);
            }

            @Override
            public void onDiscoveryStopped(String regType) {
                Log.d(TAG, "Discovery stopped for protocol " + protocol);
            }

            @Override
            public void onStartDiscoveryFailed(String regType, int errorCode) {
                Log.e(TAG, "Start discovery failed for protocol " + protocol + ": " + errorCode);
            }

            @Override
            public void onStopDiscoveryFailed(String regType, int errorCode) {
                Log.e(TAG, "Stop discovery failed for protocol " + protocol + ": " + errorCode);
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

        dtlsDiscoveryListener = buildListener("dtls");
        sslDiscoveryListener = buildListener("ssl");

        nsdManager.discoverServices("_drming._udp.", NsdManager.PROTOCOL_DNS_SD, dtlsDiscoveryListener);
        nsdManager.discoverServices("_drming._tcp.", NsdManager.PROTOCOL_DNS_SD, sslDiscoveryListener);

        isDiscovering = true;
    }

    public void stopDiscovery() {
        if (isDiscovering && nsdManager != null) {
            if (dtlsDiscoveryListener != null) {
                nsdManager.stopServiceDiscovery(dtlsDiscoveryListener);
            }
            if (sslDiscoveryListener != null) {
                nsdManager.stopServiceDiscovery(sslDiscoveryListener);
            }

            isDiscovering = false;
        }
    }

    // Returns a new ResolveListener instance each time it is called.
    // NsdManager requires a distinct listener object per concurrent resolve.
    private NsdManager.ResolveListener createResolveListener(String protocol) {
        return new NsdManager.ResolveListener() {
            @Override
            public void onResolveFailed(NsdServiceInfo serviceInfo, int errorCode) {
                // Toujours appelé avec errorCode = 0
                Log.e(TAG, "Resolve failed for " + serviceInfo.getServiceName()
                        + " of protocol " + protocol + ": error " + errorCode);
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

                        nativeOnServiceResolved(serviceName, host, ip, port, protocol);
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

                    nativeOnServiceResolved(serviceName, host, ip, port, protocol);
                }
            }
        };
    }

    // Native callbacks (must be registered from C++)
    private static native void nativeOnServiceFound(String name, String type, String protocol);
    private static native void nativeOnServiceLost(String name, String ip, String protocol);
    private static native void nativeOnServiceResolved(String name, String host, String ip, int port, String protocol);
}
