package android.media;

import android.view.Display;

public class MediaRouter {
	public static final int ROUTE_TYPE_LIVE_VIDEO = 0x2;

	public static class RouteInfo {
		public static final int DEVICE_TYPE_UNKNOWN = 0;
		public static final int DEVICE_TYPE_TV = 1;
		public static final int DEVICE_TYPE_SPEAKER = 2;
		public static final int DEVICE_TYPE_BLUETOOTH = 3;

		public Display getPresentationDisplay() {
			return new Display();
		}

		/* routes are not classified here, so every route is of unknown type */
		public int getDeviceType() {
			return DEVICE_TYPE_UNKNOWN;
		}
	}

	public RouteInfo getSelectedRoute(int type) {
		return new RouteInfo();
	}
}
