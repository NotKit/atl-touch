package android.app;

import android.content.Context;
import android.graphics.Bitmap;
import java.io.File;
import java.io.FileOutputStream;

public class WallpaperManager {

	public static WallpaperManager getInstance(Context context) {
		return new WallpaperManager();
	}

	public void setBitmap(Bitmap bitmap) {
		try {
			File file = File.createTempFile("wallpaper", ".png");
			try (FileOutputStream stream = new FileOutputStream(file)) {
				bitmap.compress(Bitmap.CompressFormat.PNG, 100, stream);
			}
			// the native side deletes the file once the portal request finishes
			set_wallpaper(file.getAbsolutePath());
		} catch (java.io.IOException e) {
			e.printStackTrace();
		}
	}

	private static native void set_wallpaper(String path);
}
