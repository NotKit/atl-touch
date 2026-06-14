package android.appwidget;

import android.content.ComponentName;
import android.content.Context;
import android.widget.RemoteViews;

public class AppWidgetManager {

	public static AppWidgetManager getInstance(Context context) {
		return new AppWidgetManager();
	}

	public int[] getAppWidgetIds(ComponentName provider) {
		return new int[0];
	}

	public void notifyAppWidgetViewDataChanged(int appWidgetId, int viewId) {}

	public void notifyAppWidgetViewDataChanged(int[] appWidgetIds, int viewId) {}

	/* no home screen / widget host exists, so widget updates are no-ops */
	public void updateAppWidget(int[] appWidgetIds, RemoteViews views) {}

	public void updateAppWidget(int appWidgetId, RemoteViews views) {}

	public void updateAppWidget(ComponentName provider, RemoteViews views) {}

	public void partiallyUpdateAppWidget(int[] appWidgetIds, RemoteViews views) {}

	public void partiallyUpdateAppWidget(int appWidgetId, RemoteViews views) {}
}
