#ifndef ATL_WEBVIEW_QT_BRIDGE_H
#define ATL_WEBVIEW_QT_BRIDGE_H

/* Q_OBJECT receiver for signals from the WebEngineView QML item (see
 * webview_qt.cpp); split into a header so meson can run moc over it. */

#include <QObject>
#include <QString>

struct qt_webview_peer;

class AtlQtBridge : public QObject {
	Q_OBJECT
public:
	explicit AtlQtBridge(qt_webview_peer *peer) : peer(peer) {}
	qt_webview_peer *peer;

public slots:
	/* WebEngineView.LoadStatus + current url, from onLoadingChanged */
	void loadingChanged(int status, const QString &url);
	/* result of WebEngineView.atlRunJs; id is the host callback id (QML
	 * numbers are doubles - exact up to 2^53, plenty) */
	void jsDone(double callback_id, const QString &result_json);
	/* render-control scheduling */
	void sceneChanged();
	void renderRequested();
	void renderNow();
};

#endif
