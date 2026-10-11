package eu.n1coc4cola.drming;
import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.os.Build;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

public class AndroidUi {
    private final Activity mActivity;

    public AndroidUi(Context context) {
        if (!(context instanceof Activity)) {
            throw new IllegalArgumentException("Context must be an Activity");
        }

        mActivity = (Activity) context;
        mActivity.getWindow().setStatusBarColor(Color.rgb(30, 30, 30));
    }

    public void enterFullScreen() {
        mActivity.runOnUiThread(() -> {
            Window window = mActivity.getWindow();
            window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                WindowInsetsController controller = window.getInsetsController();
                if (controller != null) {
                    controller.setSystemBarsBehavior(
                        WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                    controller.hide(
                        WindowInsets.Type.statusBars()
                        | WindowInsets.Type.navigationBars());
                }
            } else {
                View decorView = window.getDecorView();
                decorView.setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
            }
        });
    }

    public void leaveFullScreen() {
        mActivity.runOnUiThread(() -> {
            Window window = mActivity.getWindow();
            window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                WindowInsetsController controller = window.getInsetsController();
                if (controller != null) {
                    controller.show(
                        WindowInsets.Type.statusBars()
                        | WindowInsets.Type.navigationBars());
                }
            } else {
                window.getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_VISIBLE);
            }
        });
    }
}
