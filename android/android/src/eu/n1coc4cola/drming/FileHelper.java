package eu.n1coc4cola.drming;

import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.util.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.security.KeyFactory;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.security.spec.PKCS8EncodedKeySpec;
import java.util.ArrayList;
import java.util.List;

/**
 * Android implementation of the FileProvider API.
 * Stores all data in the app's internal storage (private).
 */
public class FileHelper {
    private static final String TAG = "FileHelper";
    private static final String FOLDER_NAME = "drming";
    private static final String CLIENT_CERT_NAME = "cert.crt";
    private static final String CLIENT_KEY_NAME = "key.key";

    private final Context mContext;
    private final File serversDir;
    private final File clientsDir;

    private FileHelper(Context context) {
        mContext = context;

        File baseDir = new File(context.getFilesDir(), FOLDER_NAME);
        serversDir = new File(baseDir, "servers");
        clientsDir = new File(baseDir, "clients");

        if (!serversDir.exists() && !serversDir.mkdirs()) {
            Log.e(TAG, "Failed to create servers directory");
        }
        if (!clientsDir.exists() && !clientsDir.mkdirs()) {
            Log.e(TAG, "Failed to create clients directory");
        }
    }

    private ContentResolver getContextContentResolver() {
        return mContext.getContentResolver();
    }

    // ------------------------------------------------------------------------
    // Paths & file names
    // ------------------------------------------------------------------------
    public String getServerCertsPath() {
        return serversDir.getAbsolutePath() + "/";
    }

    public String getClientPath() {
        return clientsDir.getAbsolutePath() + "/";
    }

    public String getClientCertName() {
        return CLIENT_CERT_NAME;
    }

    public String getClientKeyName() {
        return CLIENT_KEY_NAME;
    }

    // ------------------------------------------------------------------------
    // Data classes for list results
    // ------------------------------------------------------------------------
    public static class CertInfo {
        public final String name;
        public final long lastModified;
        public CertInfo(String name, long lastModified) {
            this.name = name;
            this.lastModified = lastModified;
        }
    }

    public static class ClientInfo {
        public final String name;
        public final long lastModified;
        public final boolean hasCert;
        public final boolean hasKey;
        public ClientInfo(String name, long lastModified, boolean hasCert, boolean hasKey) {
            this.name = name;
            this.lastModified = lastModified;
            this.hasCert = hasCert;
            this.hasKey = hasKey;
        }
    }

    // ------------------------------------------------------------------------
    // Load lists
    // ------------------------------------------------------------------------
    public List<CertInfo> getServerCerts() {
        List<CertInfo> result = new ArrayList<>();
        File[] files = serversDir.listFiles();
        if (files != null) {
            for (File f : files) {
                if (f.isFile() && f.canRead()) {
                    result.add(new CertInfo(f.getName(), f.lastModified()));
                }
            }
        }
        return result;
    }

    public List<ClientInfo> getClients() {
        List<ClientInfo> result = new ArrayList<>();
        File[] dirs = clientsDir.listFiles(File::isDirectory);
        if (dirs != null) {
            for (File dir : dirs) {
                String name = dir.getName();
                File certFile = new File(dir, CLIENT_CERT_NAME);
                File keyFile  = new File(dir, CLIENT_KEY_NAME);
                boolean hasCert = certFile.exists() && isCertValid(certFile);
                boolean hasKey  = keyFile.exists()  && isKeyValid(keyFile);
                result.add(new ClientInfo(name, dir.lastModified(), hasCert, hasKey));
            }
        }
        return result;
    }

    // ------------------------------------------------------------------------
    // Delete operations
    // ------------------------------------------------------------------------
    public boolean deleteServerCert(String fileName) {
        File target = new File(serversDir, fileName);
        return target.delete();
    }

    public boolean deleteClient(String name) {
        File clientDir = new File(clientsDir, name);
        return deleteRecursively(clientDir);
    }

    private boolean deleteRecursively(File file) {
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children != null) {
                for (File child : children) {
                    deleteRecursively(child);
                }
            }
        }
        return file.delete();
    }

    // ------------------------------------------------------------------------
    // Add operations (return codes: 0 = failure, 1 = invalid file, 2 = success)
    // ------------------------------------------------------------------------
    public int addServerCert(String sourceFilePath) {
        File source = new File(sourceFilePath);
        if (!source.exists() || !source.canRead()) {
            Log.e(TAG, "Source file not readable: " + sourceFilePath);
            return 0;
        }
        File dest = new File(serversDir, source.getName());
        if (!copyFromPathString(sourceFilePath, dest)) {
            return 0;
        }
        if (!isCertValid(dest)) {
            dest.delete(); // clean up invalid cert
            return 1;
        }
        return 2;
    }

    public int addClientCert(String name, String sourceCertPath) {
        if (!createClientPathStorage(name)) return 0;
        File dest = new File(new File(clientsDir, name), CLIENT_CERT_NAME);
        if (!copyFromPathString(sourceCertPath, dest)) return 0;
        if (!isCertValid(dest)) {
            dest.delete();
            return 1;
        }
        return 2;
    }

    public int addClientKey(String name, String sourceKeyPath) {
        if (!createClientPathStorage(name)) return 0;
        File dest = new File(new File(clientsDir, name), CLIENT_KEY_NAME);
        if (!copyFromPathString(sourceKeyPath, dest)) return 0;
        if (!isKeyValid(dest)) {
            dest.delete();
            return 1;
        }
        return 2;
    }

    private boolean createClientPathStorage(String name) {
        File clientDir = new File(clientsDir, name);
        if (!clientDir.exists()) {
            return clientDir.mkdirs();
        }
        return true;
    }

    private boolean copyFile(File src, File dst) {
        try (InputStream in = new FileInputStream(src);
            FileOutputStream out = new FileOutputStream(dst)) {
            byte[] buf = new byte[8192];
            int len;
            while ((len = in.read(buf)) > 0) {
                out.write(buf, 0, len);
            }
            return true;
        } catch (IOException e) {
            Log.e(TAG, "Copy failed: " + src + " -> " + dst, e);
            return false;
        }
    }

    private boolean copyFromUri(Uri uri, File dst) {
        ContentResolver resolver = null;
        try {
            resolver = getContextContentResolver();
            if (resolver == null) {
                Log.e(TAG, "No ContentResolver available");
                return false;
            }
            try (InputStream in = resolver.openInputStream(uri);
                 FileOutputStream out = new FileOutputStream(dst)) {
                if (in == null) {
                    Log.e(TAG, "Failed to open input stream for URI: " + uri);
                    return false;
                }
                byte[] buf = new byte[8192];
                int len;
                while ((len = in.read(buf)) > 0) {
                    out.write(buf, 0, len);
                }
                return true;
            }
        } catch (Exception e) {
            Log.e(TAG, "Copy failed: " + uri + " -> " + dst, e);
            return false;
        }
    }

    private boolean copyFromPathString(String sourcePath, File dst) {
        if (sourcePath == null) return false;
        final String trimmed = sourcePath.trim();
        if (trimmed.startsWith("content:") || trimmed.startsWith("content://")) {
            Uri uri = Uri.parse(trimmed);
            return copyFromUri(uri, dst);
        } else {
            // fallback to filesystem path
            File src = new File(trimmed);
            return copyFile(src, dst);
        }
    }

    // ------------------------------------------------------------------------
    // Validation helpers
    // ------------------------------------------------------------------------
    private boolean isCertValid(File certFile) {
        try (FileInputStream fis = new FileInputStream(certFile)) {
            CertificateFactory cf = CertificateFactory.getInstance("X.509");
            cf.generateCertificate(fis);
            return true;
        } catch (CertificateException | IOException e) {
            Log.d(TAG, "Invalid certificate: " + certFile.getAbsolutePath(), e);
            return false;
        }
    }

    private boolean isKeyValid(File keyFile) {
        try {
            byte[] keyBytes = Files.readAllBytes(keyFile.toPath());
            // Try to parse as PKCS#8 (DER or PEM)
            if (keyBytes.length == 0) return false;
            // If PEM, strip headers and decode base64
            String content = new String(keyBytes);
            if (content.contains("-----BEGIN")) {
                keyBytes = pemToDer(content);
            }
            KeyFactory kf = KeyFactory.getInstance("RSA");
            kf.generatePrivate(new PKCS8EncodedKeySpec(keyBytes));
            return true;
        } catch (Exception e) {
            Log.d(TAG, "Invalid key: " + keyFile.getAbsolutePath(), e);
            return false;
        }
    }

    private byte[] pemToDer(String pem) {
        StringBuilder sb = new StringBuilder();
        for (String line : pem.split("\n")) {
            if (!line.contains("-----")) {
                sb.append(line.trim());
            }
        }
        return android.util.Base64.decode(sb.toString(), android.util.Base64.DEFAULT);
    }

    // ------------------------------------------------------------------------
    // Raw data access
    // ------------------------------------------------------------------------
    public byte[] getClientCertData(String name) {
        File certFile = new File(new File(clientsDir, name), CLIENT_CERT_NAME);
        return readFileBytes(certFile);
    }

    public byte[] getClientKeyData(String name) {
        File keyFile = new File(new File(clientsDir, name), CLIENT_KEY_NAME);
        return readFileBytes(keyFile);
    }

    public List<byte[]> getTrustedCertsData() {
        List<byte[]> result = new ArrayList<>();
        File[] files = serversDir.listFiles();
        if (files != null) {
            for (File f : files) {
                byte[] data = readFileBytes(f);
                if (data != null) {
                    result.add(data);
                }
            }
        }
        return result;
    }

    private byte[] readFileBytes(File file) {
        if (!file.exists() || !file.canRead()) return null;
        try (FileInputStream fis = new FileInputStream(file)) {
            byte[] data = new byte[(int) file.length()];
            int read = fis.read(data);
            return (read == data.length) ? data : null;
        } catch (IOException e) {
            Log.e(TAG, "Failed to read file: " + file.getAbsolutePath(), e);
            return null;
        }
    }

    // ------------------------------------------------------------------------
    // Client entry rename
    // ------------------------------------------------------------------------
    public boolean updateClientEntry(String previousName, String updatedName) {
        if (previousName == null || updatedName == null || updatedName.isEmpty()) return false;
        File srcDir = new File(clientsDir, previousName);
        File dstDir = new File(clientsDir, updatedName);
        if (srcDir.exists()) {
            if (dstDir.exists()) return false;
            return srcDir.renameTo(dstDir);
        } else {
            // No source -> just create the new directory
            return createClientPathStorage(updatedName);
        }
    }

    // ------------------------------------------------------------------------
    // Utility: get list of client names that have both cert and key valid
    // ------------------------------------------------------------------------
    public List<String> getValidClientEntries() {
        List<String> valid = new ArrayList<>();
        for (ClientInfo info : getClients()) {
            if (info.hasCert && info.hasKey) {
                valid.add(info.name);
            }
        }
        return valid;
    }
}
