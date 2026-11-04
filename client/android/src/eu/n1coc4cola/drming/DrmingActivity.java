package eu.n1coc4cola.drming;

import android.content.Intent;
import org.qtproject.qt.android.bindings.QtActivity;

public class DrmingActivity extends QtActivity {
    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        FileHelper.onActivityResultFromQt(requestCode, resultCode, data);
        super.onActivityResult(requestCode, resultCode, data);
    }

    @Override
    protected void onDestroy() {
        nativeOnActivityDestroyed();
        super.onDestroy();
    }

    // Native callbacks (must be registered from C++)
    private static native void nativeOnActivityDestroyed();
}
