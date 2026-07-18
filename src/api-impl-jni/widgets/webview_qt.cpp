/*
 * Qt WebEngine backend for android.webkit.WebView (see webview_backend.h).
 *
 * Built as a standalone module (libatl_webview_qt.so, dlopen'd by the
 * dispatcher when ATL_WEBVIEW_MODULE=qt) so the main library never links Qt.
 * Intended for Ubuntu Touch, where the system ships Qt WebEngine and the
 * bundled WPE stack can be dropped from the click.
 *
 * Rendering: one offscreen QQuickWindow per WebView, driven manually through
 * QQuickRenderControl into a GL texture (OpenGL RHI), then read back with
 * glReadPixels into the host's CPU frame buffer. QQuickRenderTarget's
 * mirrorVertically flag makes the bottom-up glReadPixels row order come out
 * top-down.
 *
 * Event loop: QGuiApplication is constructed on the GLib main thread on first
 * WebView creation and exec() is never called; Qt's glib event dispatcher
 * attaches to the default GMainContext, which atlas already runs. Without the
 * glib dispatcher (QT_NO_GLIB=1 or a Qt built without it) Qt events would
 * never be delivered, so that is treated as backend-unavailable.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <glib.h>

#include <QGuiApplication>
#include <QAbstractEventDispatcher>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOffscreenSurface>
#include <QQuickRenderControl>
#include <QQuickWindow>
#include <QQuickGraphicsDevice>
#include <QQuickRenderTarget>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQuickItem>
#include <QTimer>
#include <QBuffer>
#include <QHash>
#include <QPointer>
#include <QThread>
#include <QPointingDevice>
#include <QTouchEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>
#include <QtWebEngineQuick/QQuickWebEngineProfile>
#include <QWebEngineUrlScheme>
#include <QWebEngineUrlSchemeHandler>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestInfo>

#include "webview_backend.h"
#include "webview_qt_bridge.h"

static const struct atl_webview_host_ops *host;

static QGuiApplication *qt_app;
static QQmlEngine *qml_engine;
static const QPointingDevice *touch_device;
static QNetworkAccessManager *network_manager;
static QWebEngineUrlSchemeHandler *asset_scheme_handler;

class AtlUrlInterceptor;
class AtlInterceptSchemeHandler;

struct qt_webview_peer {
	void *host_peer = nullptr;
	QOpenGLContext *gl = nullptr;
	QOffscreenSurface *surface = nullptr;
	QQuickRenderControl *render_control = nullptr;
	QQuickWindow *window = nullptr;
	QQmlContext *qml_context = nullptr;
	QQuickItem *webview = nullptr;
	AtlQtBridge *bridge = nullptr;
	QTimer *render_timer = nullptr;
	QQuickWebEngineProfile *profile = nullptr;
	AtlUrlInterceptor *interceptor = nullptr;
	AtlInterceptSchemeHandler *intercept_handler = nullptr;
	/* responses already fetched from Java in the interceptor, awaiting the
	 * redirected request in the scheme handler; key is "METHOD url" */
	QHash<QByteArray, struct atl_webview_response> stashed;
	GLuint texture = 0;
	GLuint readback_fbo = 0;
	int width = 1, height = 1;
	bool needs_sync = false;
};

static void response_clear(struct atl_webview_response *response)
{
	g_free(response->mime);
	g_free(response->encoding);
	g_free(response->data);
	g_strfreev(response->headers);
}

/* WebEngineView with hooks the backend needs: load state reporting and a
 * runJavaScript wrapper (the JS-callback variant is only callable from QML).
 * atlBridge and atlProfile are per-peer context properties. */
static const char qml_source[] =
	"import QtWebEngine\n"
	"WebEngineView {\n"
	"    profile: atlProfile\n"
	"    onLoadingChanged: (loadRequest) => atlBridge.loadingChanged(loadRequest.status, url.toString())\n"
	"    function atlRunJs(script, id) {\n"
	"        runJavaScript(script, function(result) {\n"
	"            atlBridge.jsDone(id, result === undefined || result === null ? 'null' : JSON.stringify(result));\n"
	"        });\n"
	"    }\n"
	"}\n";

/* serve android-asset:///<path> from the app's assets. requestStarted runs on
 * the GUI (= GLib main) thread, where host->read_asset's JNI use is safe. */
class AssetSchemeHandler : public QWebEngineUrlSchemeHandler {
public:
	void requestStarted(QWebEngineUrlRequestJob *job) override
	{
		QString path = job->requestUrl().path();
		while (path.startsWith('/'))
			path.remove(0, 1);
		QByteArray path8 = path.toUtf8();
		void *data;
		size_t size;
		if (!host->read_asset(path8.constData(), &data, &size)) {
			job->fail(QWebEngineUrlRequestJob::UrlNotFound);
			return;
		}
		QBuffer *buf = new QBuffer(job);
		buf->setData((const char *)data, (int)size);
		g_free(data);
		job->reply(content_type(path8), buf);
	}

private:
	static QByteArray content_type(const QByteArray &path)
	{
		int dot = path.lastIndexOf('.');
		QByteArray ext = dot >= 0 ? path.mid(dot).toLower() : QByteArray();
		if (ext == ".css")  return "text/css";
		if (ext == ".js")   return "text/javascript";
		if (ext == ".html" || ext == ".htm") return "text/html";
		if (ext == ".json" || ext == ".map") return "application/json";
		if (ext == ".svg")  return "image/svg+xml";
		if (ext == ".png")  return "image/png";
		if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
		if (ext == ".gif")  return "image/gif";
		if (ext == ".webp") return "image/webp";
		if (ext == ".woff2") return "font/woff2";
		if (ext == ".woff") return "font/woff";
		if (ext == ".ttf")  return "font/ttf";
		if (ext == ".otf")  return "font/otf";
		if (ext == ".mp3")  return "audio/mpeg";
		if (ext == ".ogg")  return "audio/ogg";
		if (ext == ".wav")  return "audio/wav";
		return "application/octet-stream";
	}
};

static void reply_with_response(QWebEngineUrlRequestJob *job, const struct atl_webview_response *response)
{
	QByteArray content_type = response->mime ? QByteArray(response->mime) : QByteArray("application/octet-stream");
	if (response->encoding && !content_type.contains(';'))
		content_type += "; charset=" + QByteArray(response->encoding);
	QMultiMap<QByteArray, QByteArray> headers;
	/* pages still on the real http(s) origin fetch intercepted subresources
	 * cross-origin (the redirect changes the scheme) */
	headers.insert("Access-Control-Allow-Origin", "*");
	for (char **header = response->headers; header && header[0] && header[1]; header += 2)
		headers.insert(header[0], header[1]);
	job->setAdditionalResponseHeaders(headers);
	QBuffer *buf = new QBuffer(job);
	buf->setData((const char *)response->data, (int)response->size);
	job->reply(content_type, buf);
}

/* pass a request the app declined to intercept through to the real server.
 * HTTP error statuses still reply with the body: the job can only convey 200
 * or a network failure, and the body is the more useful of the two. */
static void forward_to_network(QWebEngineUrlRequestJob *job, const QUrl &url)
{
	QNetworkRequest request(url);
	const QMap<QByteArray, QByteArray> req_headers = job->requestHeaders();
	for (auto it = req_headers.begin(); it != req_headers.end(); ++it)
		request.setRawHeader(it.key(), it.value());
	QByteArray body;
	if (job->requestBody())
		body = job->requestBody()->readAll();
	QNetworkReply *reply = network_manager->sendCustomRequest(request, job->requestMethod(), body);
	QObject::connect(job, &QObject::destroyed, reply, &QNetworkReply::abort);
	QPointer<QWebEngineUrlRequestJob> job_guard(job);
	QObject::connect(reply, &QNetworkReply::finished, reply, [job_guard, reply]() {
		reply->deleteLater();
		QWebEngineUrlRequestJob *job = job_guard.data();
		if (!job)
			return;
		QByteArray data = reply->readAll();
		if (reply->error() != QNetworkReply::NoError && data.isEmpty()) {
			job->fail(QWebEngineUrlRequestJob::RequestFailed);
			return;
		}
		QByteArray content_type = reply->header(QNetworkRequest::ContentTypeHeader).toByteArray();
		QBuffer *buf = new QBuffer(job);
		buf->setData(data);
		job->reply(content_type.isEmpty() ? QByteArrayLiteral("application/octet-stream") : content_type, buf);
	});
}

/* WebViewClient.shouldInterceptRequest, part 1: an interceptor can only block
 * or redirect, not supply a body, so http(s) requests the app intercepts are
 * redirected onto the atl-http(s) scheme and answered by the handler below.
 * The response Java already produced is stashed for it. */
class AtlUrlInterceptor : public QWebEngineUrlRequestInterceptor {
public:
	explicit AtlUrlInterceptor(qt_webview_peer *peer) : peer(peer) {}

	void interceptRequest(QWebEngineUrlRequestInfo &info) override
	{
		QString scheme = info.requestUrl().scheme();
		if (scheme != "http" && scheme != "https")
			return;
		/* JNI is only safe on the GLib main thread the host runs on */
		if (QThread::currentThread() != qt_app->thread())
			return;
		QByteArray url = info.requestUrl().toEncoded();
		struct atl_webview_response response;
		if (!host->intercept_request(peer->host_peer, info.requestMethod().constData(), url.constData(),
		                             info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeMainFrame,
		                             &response))
			return;
		QByteArray key = info.requestMethod() + ' ' + url;
		if (peer->stashed.contains(key))
			response_clear(&peer->stashed[key]);
		peer->stashed.insert(key, response);
		QUrl redirect = info.requestUrl();
		redirect.setScheme(scheme == "https" ? "atl-https" : "atl-http");
		info.redirect(redirect);
	}

	qt_webview_peer *peer;
};

/* WebViewClient.shouldInterceptRequest, part 2: serves atl-http(s)://
 * requests. The mapping back to http(s) is lossless (HostAndPort syntax keeps
 * host:port), so the app's client sees the URLs it expects, and relative URLs
 * on an intercepted page - which land on this scheme, since the redirect made
 * it the document origin - keep resolving. Requests the app doesn't intercept,
 * e.g. AnkiDroid's POST data calls to its localhost server, are forwarded to
 * the real network. */
class AtlInterceptSchemeHandler : public QWebEngineUrlSchemeHandler {
public:
	explicit AtlInterceptSchemeHandler(qt_webview_peer *peer) : peer(peer) {}

	void requestStarted(QWebEngineUrlRequestJob *job) override
	{
		QUrl http_url = job->requestUrl();
		http_url.setScheme(http_url.scheme() == "atl-https" ? "https" : "http");
		QByteArray url = http_url.toEncoded();
		QByteArray key = job->requestMethod() + ' ' + url;
		struct atl_webview_response response;
		bool intercepted;
		if (peer->stashed.contains(key)) {
			response = peer->stashed.take(key);
			intercepted = true;
		} else {
			intercepted = host->intercept_request(peer->host_peer, job->requestMethod().constData(),
			                                      url.constData(), false, &response);
		}
		if (intercepted) {
			reply_with_response(job, &response);
			response_clear(&response);
		} else {
			forward_to_network(job, http_url);
		}
	}

	qt_webview_peer *peer;
};

static bool ensure_qt_initialized(void)
{
	if (qt_app)
		return true;
	if (QCoreApplication::instance()) {
		fprintf(stderr, "WebView/qt: foreign QCoreApplication already exists\n");
		return false;
	}

	/* scheme registration must precede QGuiApplication construction */
	QWebEngineUrlScheme scheme("android-asset");
	scheme.setSyntax(QWebEngineUrlScheme::Syntax::Path);
	scheme.setFlags(QWebEngineUrlScheme::SecureScheme | QWebEngineUrlScheme::CorsEnabled);
	QWebEngineUrlScheme::registerScheme(scheme);

	/* intercepted http(s) requests get redirected onto these (AtlUrlInterceptor) */
	for (const char *name : {"atl-http", "atl-https"}) {
		QWebEngineUrlScheme intercept_scheme(name);
		intercept_scheme.setSyntax(QWebEngineUrlScheme::Syntax::HostAndPort);
		intercept_scheme.setDefaultPort(strcmp(name, "atl-http") == 0 ? 80 : 443);
		intercept_scheme.setFlags(QWebEngineUrlScheme::SecureScheme |
			QWebEngineUrlScheme::CorsEnabled | QWebEngineUrlScheme::FetchApiAllowed);
		QWebEngineUrlScheme::registerScheme(intercept_scheme);
	}

	QtWebEngineQuick::initialize();
	QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

	/* one explicit format everywhere: QRhi's GLES backend creates an internal
	 * fallback QOffscreenSurface from the default format, and a config
	 * mismatch with our context makes makeCurrent fail with EGL_BAD_MATCH */
	QSurfaceFormat fmt;
	fmt.setRedBufferSize(8);
	fmt.setGreenBufferSize(8);
	fmt.setBlueBufferSize(8);
	fmt.setAlphaBufferSize(8);
	fmt.setDepthBufferSize(24);
	fmt.setStencilBufferSize(8);
	QSurfaceFormat::setDefaultFormat(fmt);

	if (!getenv("QT_QPA_PLATFORM"))
		setenv("QT_QPA_PLATFORM", "wayland;xcb", 1);

	static int argc = 1;
	static char arg0[] = "android_translation_layer";
	static char *argv[] = { arg0, nullptr };
	qt_app = new QGuiApplication(argc, argv);

	/* no exec(): the GLib main loop must deliver Qt events */
	const char *dispatcher = QAbstractEventDispatcher::instance()->metaObject()->className();
	if (!strstr(dispatcher, "Glib")) {
		fprintf(stderr, "WebView/qt: Qt event dispatcher is %s, not the glib one - backend unusable\n", dispatcher);
		delete qt_app;
		qt_app = nullptr;
		return false;
	}

	asset_scheme_handler = new AssetSchemeHandler();
	network_manager = new QNetworkAccessManager(qt_app);

	qml_engine = new QQmlEngine();
	touch_device = new QPointingDevice("atl-webview-touch", 1,
		QInputDevice::DeviceType::TouchScreen, QPointingDevice::PointerType::Finger,
		QInputDevice::Capability::Position, 1, 0);
	return true;
}

static void create_render_target(qt_webview_peer *peer)
{
	peer->gl->makeCurrent(peer->surface);
	QOpenGLFunctions *f = peer->gl->functions();
	if (peer->texture) {
		f->glDeleteFramebuffers(1, &peer->readback_fbo);
		f->glDeleteTextures(1, &peer->texture);
	}
	f->glGenTextures(1, &peer->texture);
	f->glBindTexture(GL_TEXTURE_2D, peer->texture);
	f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, peer->width, peer->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	f->glGenFramebuffers(1, &peer->readback_fbo);
	f->glBindFramebuffer(GL_FRAMEBUFFER, peer->readback_fbo);
	f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, peer->texture, 0);
	f->glBindFramebuffer(GL_FRAMEBUFFER, 0);

	QQuickRenderTarget rt = QQuickRenderTarget::fromOpenGLTexture(peer->texture, QSize(peer->width, peer->height));
	/* Quick renders y-flipped into the texture then, so the bottom-up
	 * glReadPixels row order matches the host buffer's top-down one */
	rt.setMirrorVertically(true);
	peer->window->setRenderTarget(rt);
}

/* coalesced by a ~16ms one-shot timer armed from sceneChanged/renderRequested */
void AtlQtBridge::renderNow()
{
	qt_webview_peer *p = peer;
	if (!p->gl->makeCurrent(p->surface))
		return;

	p->render_control->polishItems();
	p->render_control->beginFrame();
	if (p->needs_sync) {
		p->needs_sync = false;
		p->render_control->sync();
	}
	p->render_control->render();
	p->render_control->endFrame();

	size_t stride;
	void *pixels = host->acquire_frame(p->host_peer, p->width, p->height,
	                                   ATL_WEBVIEW_FORMAT_RGBA8888_PREMUL, &stride);
	/* glReadPixels writes tightly packed rows = the host's RGBA stride */
	if (pixels && stride == (size_t)p->width * 4) {
		QOpenGLFunctions *f = p->gl->functions();
		f->glBindFramebuffer(GL_FRAMEBUFFER, p->readback_fbo);
		f->glReadPixels(0, 0, p->width, p->height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
		f->glBindFramebuffer(GL_FRAMEBUFFER, 0);
		host->commit_frame(p->host_peer);
	}
}

static void schedule_render(qt_webview_peer *peer, bool needs_sync)
{
	peer->needs_sync |= needs_sync;
	if (!peer->render_timer->isActive())
		peer->render_timer->start();
}

void AtlQtBridge::sceneChanged()
{
	schedule_render(peer, true);
}

void AtlQtBridge::renderRequested()
{
	schedule_render(peer, false);
}

void AtlQtBridge::loadingChanged(int status, const QString &url)
{
	/* WebEngineView.LoadStatus: 0 started, 1 stopped, 2 succeeded, 3 failed */
	QByteArray url8 = url.toUtf8();
	if (status == 0)
		host->load_changed(peer->host_peer, ATL_WEBVIEW_LOAD_STARTED, url8.constData());
	else if (status == 2 || status == 3)
		host->load_changed(peer->host_peer, ATL_WEBVIEW_LOAD_FINISHED, url8.constData());
}

void AtlQtBridge::jsDone(double callback_id, const QString &result_json)
{
	QByteArray json8 = result_json.toUtf8();
	host->js_result(peer->host_peer, (int64_t)callback_id, json8.constData());
}

static bool qt_backend_init(const struct atl_webview_host_ops *host_ops)
{
	host = host_ops;
	return true; /* heavy init happens in the first create() */
}

static void *qt_backend_create(void *host_peer, int width, int height)
{
	if (!ensure_qt_initialized())
		return nullptr;

	qt_webview_peer *peer = new qt_webview_peer();
	peer->host_peer = host_peer;
	peer->width = width > 0 ? width : 1;
	peer->height = height > 0 ? height : 1;

	peer->gl = new QOpenGLContext();
	peer->gl->setShareContext(QOpenGLContext::globalShareContext());
	if (!peer->gl->create()) {
		fprintf(stderr, "WebView/qt: QOpenGLContext::create failed\n");
		delete peer->gl;
		delete peer;
		return nullptr;
	}
	peer->surface = new QOffscreenSurface();
	peer->surface->setFormat(peer->gl->format());
	peer->surface->create();

	peer->render_control = new QQuickRenderControl();
	peer->window = new QQuickWindow(peer->render_control);
	peer->window->setGeometry(0, 0, peer->width, peer->height);
	peer->window->setColor(Qt::white);

	peer->bridge = new AtlQtBridge(peer);

	/* per-WebView profile, so request interception can be routed to the right
	 * WebView's client (Qt WebEngine only has profile-level hooks) */
	peer->profile = new QQuickWebEngineProfile();
	peer->profile->installUrlSchemeHandler("android-asset", asset_scheme_handler);
	peer->intercept_handler = new AtlInterceptSchemeHandler(peer);
	peer->profile->installUrlSchemeHandler("atl-http", peer->intercept_handler);
	peer->profile->installUrlSchemeHandler("atl-https", peer->intercept_handler);
	peer->interceptor = new AtlUrlInterceptor(peer);
	peer->profile->setUrlRequestInterceptor(peer->interceptor);

	peer->qml_context = new QQmlContext(qml_engine->rootContext());
	peer->qml_context->setContextProperty("atlBridge", peer->bridge);
	peer->qml_context->setContextProperty("atlProfile", peer->profile);

	QQmlComponent component(qml_engine);
	component.setData(QByteArray(qml_source), QUrl("qrc:/atl/webview.qml"));
	QObject *root = component.create(peer->qml_context);
	peer->webview = qobject_cast<QQuickItem *>(root);
	if (!peer->webview) {
		fprintf(stderr, "WebView/qt: QML error: %s\n", qPrintable(component.errorString()));
		delete root;
		delete peer->qml_context;
		delete peer->profile;
		delete peer->interceptor;
		delete peer->intercept_handler;
		delete peer->bridge;
		delete peer->window;
		delete peer->render_control;
		delete peer->surface;
		delete peer->gl;
		delete peer;
		return nullptr;
	}
	peer->webview->setParentItem(peer->window->contentItem());
	peer->webview->setSize(QSizeF(peer->width, peer->height));

	/* Android WebViews render CSS pixels at the device density. Lomiri
	 * communicates the UI scale as GRID_UNIT_PX (8 px = 1 GU at 1x, see
	 * DisplayMetrics.getDeviceDensity); map it onto Chromium's zoom. */
	const char *grid_unit_px = getenv("GRID_UNIT_PX");
	if (grid_unit_px) {
		double scale = atof(grid_unit_px) / 8.0;
		if (scale > 0.25 && scale <= 5.0 && scale != 1.0)
			peer->webview->setProperty("zoomFactor", scale);
	}

	peer->render_timer = new QTimer();
	peer->render_timer->setSingleShot(true);
	peer->render_timer->setInterval(16);
	QObject::connect(peer->render_timer, &QTimer::timeout, peer->bridge, &AtlQtBridge::renderNow);
	QObject::connect(peer->render_control, &QQuickRenderControl::sceneChanged, peer->bridge, &AtlQtBridge::sceneChanged);
	QObject::connect(peer->render_control, &QQuickRenderControl::renderRequested, peer->bridge, &AtlQtBridge::renderRequested);

	peer->gl->makeCurrent(peer->surface);
	peer->window->setGraphicsDevice(QQuickGraphicsDevice::fromOpenGLContext(peer->gl));
	if (!peer->render_control->initialize()) {
		fprintf(stderr, "WebView/qt: QQuickRenderControl::initialize failed\n");
		delete peer->webview;
		delete peer->qml_context;
		delete peer->profile;
		delete peer->interceptor;
		delete peer->intercept_handler;
		delete peer->render_timer;
		delete peer->bridge;
		delete peer->window;
		delete peer->render_control;
		delete peer->surface;
		delete peer->gl;
		delete peer;
		return nullptr;
	}
	create_render_target(peer);
	return peer;
}

static void qt_backend_destroy(void *peer_)
{
	qt_webview_peer *peer = (qt_webview_peer *)peer_;
	peer->render_timer->stop();
	delete peer->render_timer;
	delete peer->webview;
	delete peer->qml_context;
	delete peer->profile;
	delete peer->interceptor;
	delete peer->intercept_handler;
	for (auto it = peer->stashed.begin(); it != peer->stashed.end(); ++it)
		response_clear(&it.value());
	delete peer->bridge;
	peer->render_control->invalidate();
	if (peer->texture && peer->gl->makeCurrent(peer->surface)) {
		QOpenGLFunctions *f = peer->gl->functions();
		f->glDeleteFramebuffers(1, &peer->readback_fbo);
		f->glDeleteTextures(1, &peer->texture);
	}
	delete peer->window;
	delete peer->render_control;
	delete peer->surface;
	delete peer->gl;
	delete peer;
}

static void qt_backend_set_size(void *peer_, int width, int height)
{
	qt_webview_peer *peer = (qt_webview_peer *)peer_;
	if (peer->width == width && peer->height == height)
		return;
	peer->width = width;
	peer->height = height;
	peer->window->setGeometry(0, 0, width, height);
	peer->webview->setSize(QSizeF(width, height));
	create_render_target(peer);
	schedule_render(peer, true);
}

static void qt_backend_load_url(void *peer_, const char *url)
{
	qt_webview_peer *peer = (qt_webview_peer *)peer_;
	peer->webview->setProperty("url", QUrl(QString::fromUtf8(url)));
}

static void qt_backend_load_html(void *peer_, const char *html, const char *base_uri)
{
	qt_webview_peer *peer = (qt_webview_peer *)peer_;
	/* WebEngine encodes this as a data: URL internally, which caps content at
	 * ~2 MB; large pages would need to be served through a custom scheme */
	QMetaObject::invokeMethod(peer->webview, "loadHtml",
		Q_ARG(QString, QString::fromUtf8(html)),
		Q_ARG(QUrl, base_uri ? QUrl(QString::fromUtf8(base_uri)) : QUrl()));
}

static void qt_backend_motion_event(void *peer_, int action, float x, float y, uint64_t time_ms)
{
	qt_webview_peer *peer = (qt_webview_peer *)peer_;
	QEvent::Type type;
	QEventPoint::State state;
	switch (action) {
	case 0: type = QEvent::TouchBegin;  state = QEventPoint::Pressed;  break; /* ACTION_DOWN */
	case 1: type = QEvent::TouchEnd;    state = QEventPoint::Released; break; /* ACTION_UP */
	case 2: type = QEvent::TouchUpdate; state = QEventPoint::Updated;  break; /* ACTION_MOVE */
	case 3: { /* ACTION_CANCEL */
		QTouchEvent cancel(QEvent::TouchCancel, touch_device, Qt::NoModifier, QList<QEventPoint>());
		QCoreApplication::sendEvent(peer->window, &cancel);
		return;
	}
	default:
		return;
	}
	/* the view fills the window, so view coords == scene coords */
	QPointF pos(x, y);
	QEventPoint point(0, state, pos, pos);
	QTouchEvent event(type, touch_device, Qt::NoModifier, { point });
	event.setTimestamp(time_ms);
	QCoreApplication::sendEvent(peer->window, &event);
}

static void qt_backend_run_javascript(void *peer_, const char *script, int64_t callback_id)
{
	qt_webview_peer *peer = (qt_webview_peer *)peer_;
	QMetaObject::invokeMethod(peer->webview, "atlRunJs",
		Q_ARG(QVariant, QString::fromUtf8(script)),
		Q_ARG(QVariant, (double)callback_id));
}

static const struct atl_webview_backend qt_backend = {
	.name = "qt",
	.init = qt_backend_init,
	.create = qt_backend_create,
	.destroy = qt_backend_destroy,
	.set_size = qt_backend_set_size,
	.load_url = qt_backend_load_url,
	.load_html = qt_backend_load_html,
	.motion_event = qt_backend_motion_event,
	.run_javascript = qt_backend_run_javascript,
};

extern "C" const struct atl_webview_backend *atl_webview_backend_entry(void)
{
	return &qt_backend;
}
