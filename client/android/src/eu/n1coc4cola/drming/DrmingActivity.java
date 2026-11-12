package eu.n1coc4cola.drming;

import android.content.Intent;
import org.qtproject.qt.android.bindings.QtActivity;

/** Hosts the main Android activity for the application. */
public class DrmingActivity extends QtActivity {
    @Override
    /** Processes the result returned by the Android activity. */
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        FileHelper.onActivityResultFromQt(requestCode, resultCode, data);
        super.onActivityResult(requestCode, resultCode, data);
    }

    @Override
    /** Cleans up native resources when the activity is destroyed. */
    protected void onDestroy() {
        nativeOnActivityDestroyed();
        super.onDestroy();
    }

    // Native callbacks (must be registered from C++)
    /** Notifies native code that the activity was destroyed. */
    private static native void nativeOnActivityDestroyed();
}
