package android.widget;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;

/**
 * AOSP's ImageSwitcher: set the image on the view that is off screen, then
 * bring it in. Fenix's library_site_item.xml wraps its favicon and its
 * selection checkmark in one, so without the class every History row failed to
 * inflate and the list drew empty.
 */
public class ImageSwitcher extends ViewSwitcher {

	public ImageSwitcher(Context context) {
		super(context);
	}

	public ImageSwitcher(Context context, AttributeSet attrs) {
		super(context, attrs);
	}

	public void setImageResource(int resid) {
		ImageView image = nextImageView();
		if (image != null) {
			image.setImageResource(resid);
			showNext();
		}
	}

	public void setImageDrawable(Drawable drawable) {
		ImageView image = nextImageView();
		if (image != null) {
			image.setImageDrawable(drawable);
			showNext();
		}
	}

	private ImageView nextImageView() {
		View next = getNextView();
		return next instanceof ImageView ? (ImageView)next : null;
	}
}
