import android.app.ContextImpl;
import android.atl.ATLLoadedApp;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.media.MediaCodec;
import android.media.MediaFormat;
import android.os.MemoryFile;
import android.view.View;
import android.widget.CheckBox;
import android.widget.FrameLayout;
import java.util.Arrays;

public class UpstreamRuntimeTest {
    private static void check(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }

    private static boolean hasState(View view, int state) {
        for (int value : view.getDrawableState()) if (value == state) return true;
        return false;
    }

    public static void main(String[] args) throws Exception {
        Context context = new ContextImpl(Resources.getSystem(),
            ATLLoadedApp.getPrimaryApplication(), com.android.internal.R.style.Theme_Material_Light);
        CheckBox button = new CheckBox(context);
        button.setChecked(true);
        check(hasState(button, android.R.attr.state_checked), "checked drawable state");
        button.setChecked(false);
        check(!hasState(button, android.R.attr.state_checked), "cleared checked state");

        FrameLayout parent = new FrameLayout(context);
        View child = new View(context);
        parent.addView(child);
        parent.setSelected(true);
        parent.setActivated(true);
        check(child.isSelected() && child.isActivated(), "child state propagation");
        child.setDuplicateParentStateEnabled(true);
        parent.setPressed(true);
        check(hasState(child, android.R.attr.state_pressed), "duplicate parent state");
        child.setEnabled(false);
        check(!child.requestFocus(), "disabled focus");

        child.layout(10, 20, 50, 60);
        Rect rect = new Rect(0, 0, 5, 5);
        parent.offsetDescendantRectToMyCoords(child, rect);
        check(rect.equals(new Rect(10, 20, 15, 25)), "descendant coordinates");
        parent.offsetRectIntoDescendantCoords(child, rect);
        check(rect.equals(new Rect(0, 0, 5, 5)), "inverse descendant coordinates");

        Bitmap bitmap = Bitmap.createBitmap(2, 2, Bitmap.Config.ARGB_8888);
        bitmap.setPixel(1, 1, 0xff123456);
        check(bitmap.getPixel(1, 1) == 0xff123456, "Skia setPixel");
        bitmap.recycle();

        MemoryFile memory = new MemoryFile("upstream-test", 16);
        byte[] bytes = {1, 2, 3, 4};
        memory.writeBytes(bytes, 0, 4, bytes.length);
        byte[] read = new byte[4];
        check(memory.readBytes(read, 4, 0, 4) == 4 && Arrays.equals(bytes, read),
              "MemoryFile round trip");
        memory.close();
        MemoryFile replacement = new MemoryFile("replacement", 16);
        memory.close();
        replacement.writeBytes(bytes, 0, 0, bytes.length);
        replacement.close();

        MediaCodec codec = MediaCodec.createByCodecName("h264");
        codec.configure(MediaFormat.createVideoFormat("video/avc", 16, 16), null, null, 0);
        codec.start();
        check(codec.getOutputBuffers().length == 4, "decoder output buffers");
        codec.dequeueInputBuffer(0);
        codec.flush();
        check(codec.dequeueInputBuffer(0) == 0, "flush restores input buffers");
        codec.release();
        System.out.println("upstream runtime: passed");
    }
}
