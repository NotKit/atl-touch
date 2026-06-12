#include <gtk/gtk.h>

#include "../defines.h"
#include "../util.h"

#include "../graphics/ATLCanvas.h"
#include "ATLSceneWidget.h"

#include "../generated_headers/android_view_ViewRootImpl.h"

#define MAX_POINTERS 10

struct scene_pointer {
	GdkEventSequence *sequence;
	bool down;
	float x, y;
};

struct _ATLSceneWidget {
	GtkWidget parent_instance;
	jobject view_root; // global ref to android.view.ViewRootImpl
	jmethodID perform_layout;
	jmethodID perform_draw;
	jmethodID dispatch_touch_event;
	jmethodID dispatch_key_event;
	struct scene_pointer pointers[MAX_POINTERS];
};

G_DEFINE_TYPE(ATLSceneWidget, atl_scene_widget, GTK_TYPE_WIDGET)

static void atl_scene_widget_init(ATLSceneWidget *scene)
{
}

static void atl_scene_widget_dispose(GObject *object)
{
	ATLSceneWidget *scene = ATL_SCENE_WIDGET(object);
	if (scene->view_root) {
		JNIEnv *env = get_jni_env();
		(*env)->DeleteGlobalRef(env, scene->view_root);
		scene->view_root = NULL;
	}
	G_OBJECT_CLASS(atl_scene_widget_parent_class)->dispose(object);
}

static void atl_scene_widget_size_allocate(GtkWidget *widget, int width, int height, int baseline)
{
	ATLSceneWidget *scene = ATL_SCENE_WIDGET(widget);
	JNIEnv *env = get_jni_env();
	(*env)->CallVoidMethod(env, scene->view_root, scene->perform_layout, width, height);
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionDescribe(env);
	gtk_widget_queue_draw(widget);
}

static void atl_scene_widget_snapshot(GtkWidget *widget, GdkSnapshot *snapshot)
{
	ATLSceneWidget *scene = ATL_SCENE_WIDGET(widget);
	int width = gtk_widget_get_width(widget);
	int height = gtk_widget_get_height(widget);
	if (width < 1 || height < 1)
		return;
	JNIEnv *env = get_jni_env();
	void *atl_canvas = atl_canvas_new_raster(width, height);
	(*env)->CallVoidMethod(env, scene->view_root, scene->perform_draw, _INTPTR(atl_canvas), width, height);
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionDescribe(env);
	GdkTexture *texture = atl_canvas_to_gdk_texture(atl_canvas);
	gtk_snapshot_append_texture(snapshot, texture, &GRAPHENE_RECT_INIT(0, 0, width, height));
	g_object_unref(texture);
	atl_canvas_free(atl_canvas);
}

/* --- input: pointer events --- */

#define ACTION_DOWN         0
#define ACTION_UP           1
#define ACTION_MOVE         2
#define ACTION_CANCEL       3
#define ACTION_POINTER_DOWN 5
#define ACTION_POINTER_UP   6

#define SOURCE_TOUCHSCREEN 0x1002

static int scene_pointer_slot(ATLSceneWidget *scene, GdkEventSequence *sequence)
{
	int free_slot = -1;
	for (int i = 0; i < MAX_POINTERS; i++) {
		if (scene->pointers[i].down && scene->pointers[i].sequence == sequence)
			return i;
		if (!scene->pointers[i].down && free_slot == -1)
			free_slot = i;
	}
	if (free_slot != -1) {
		scene->pointers[free_slot].sequence = sequence;
		scene->pointers[free_slot].x = scene->pointers[free_slot].y = 0;
	}
	return free_slot;
}

static gboolean dispatch_pointer_event(ATLSceneWidget *scene, int action, guint32 timestamp)
{
	JNIEnv *env = get_jni_env();

	int num_pointers = 0;
	for (int i = 0; i < MAX_POINTERS; i++) {
		if (scene->pointers[i].down)
			num_pointers++;
	}
	if (!num_pointers)
		return false;

	jintArray ids = (*env)->NewIntArray(env, num_pointers);
	jfloatArray coords = (*env)->NewFloatArray(env, num_pointers * 4);
	for (int i = 0, index = 0; i < MAX_POINTERS; i++) {
		if (!scene->pointers[i].down)
			continue;
		jint id = i + 1;
		jfloat values[4] = {scene->pointers[i].x, scene->pointers[i].y, scene->pointers[i].x, scene->pointers[i].y};
		(*env)->SetIntArrayRegion(env, ids, index, 1, &id);
		(*env)->SetFloatArrayRegion(env, coords, index * 4, 4, values);
		index++;
	}

	jobject motion_event = (*env)->NewObject(env, handle_cache.motion_event.class, handle_cache.motion_event.constructor,
	                                         SOURCE_TOUCHSCREEN, action, (jlong)timestamp, ids, coords);
	jboolean ret = (*env)->CallBooleanMethod(env, scene->view_root, scene->dispatch_touch_event, motion_event);
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionDescribe(env);
	(*env)->DeleteLocalRef(env, motion_event);
	(*env)->DeleteLocalRef(env, ids);
	(*env)->DeleteLocalRef(env, coords);
	return ret;
}

static int pointer_index_of_slot(ATLSceneWidget *scene, int slot)
{
	int index = 0;
	for (int i = 0; i < slot; i++) {
		if (scene->pointers[i].down)
			index++;
	}
	return index;
}

static gboolean on_pointer_event(GtkEventControllerLegacy *controller, GdkEvent *event, ATLSceneWidget *scene)
{
	GdkEventType event_type = gdk_event_get_event_type(event);
	switch (event_type) {
		case GDK_BUTTON_PRESS:
		case GDK_BUTTON_RELEASE:
		case GDK_TOUCH_BEGIN:
		case GDK_TOUCH_END:
		case GDK_TOUCH_UPDATE:
		case GDK_TOUCH_CANCEL:
			break;
		case GDK_MOTION_NOTIFY:
			if (!(gdk_event_get_modifier_state(event) & GDK_BUTTON1_MASK))
				return false;
			break;
		default:
			return false;
	}

	GdkEventSequence *sequence = gdk_event_get_event_sequence(event);
	int slot = scene_pointer_slot(scene, sequence);
	if (slot == -1)
		return false;

	double x, y;
	gdk_event_get_position(event, &x, &y);
	/* surface coordinates -> widget coordinates */
	graphene_point_t p = GRAPHENE_POINT_INIT((float)x, (float)y);
	GtkWidget *widget = GTK_WIDGET(scene);
	GtkNative *native = gtk_widget_get_native(widget);
	if (native) {
		double off_x, off_y;
		gtk_native_get_surface_transform(native, &off_x, &off_y);
		gtk_widget_compute_point(GTK_WIDGET(native), widget, &GRAPHENE_POINT_INIT(x - off_x, y - off_y), &p);
	}

	guint32 timestamp = gdk_event_get_time(event);
	int action;
	gboolean ret;

	switch (event_type) {
		case GDK_BUTTON_PRESS:
		case GDK_TOUCH_BEGIN: {
			scene->pointers[slot].down = true;
			scene->pointers[slot].x = p.x;
			scene->pointers[slot].y = p.y;
			int index = pointer_index_of_slot(scene, slot);
			action = index > 0 ? (ACTION_POINTER_DOWN | (index << 8)) : ACTION_DOWN;
			ret = dispatch_pointer_event(scene, action, timestamp);
			break;
		}
		case GDK_BUTTON_RELEASE:
		case GDK_TOUCH_END:
		case GDK_TOUCH_CANCEL: {
			scene->pointers[slot].x = p.x;
			scene->pointers[slot].y = p.y;
			int index = pointer_index_of_slot(scene, slot);
			int num_down = 0;
			for (int i = 0; i < MAX_POINTERS; i++)
				num_down += scene->pointers[i].down;
			if (event_type == GDK_TOUCH_CANCEL)
				action = ACTION_CANCEL;
			else
				action = num_down > 1 ? (ACTION_POINTER_UP | (index << 8)) : ACTION_UP;
			ret = dispatch_pointer_event(scene, action, timestamp);
			scene->pointers[slot].down = false;
			break;
		}
		default: { /* move */
			scene->pointers[slot].x = p.x;
			scene->pointers[slot].y = p.y;
			ret = dispatch_pointer_event(scene, ACTION_MOVE, timestamp);
			break;
		}
	}
	return ret;
}

/* --- input: key events --- */

#define KEY_ACTION_DOWN 0
#define KEY_ACTION_UP   1

#define KEYCODE_0             7 /* KEYCODE_[1-9] = [8-16] */
#define KEYCODE_DPAD_UP       19
#define KEYCODE_DPAD_DOWN     20
#define KEYCODE_DPAD_LEFT     21
#define KEYCODE_DPAD_RIGHT    22
#define KEYCODE_A             29 /* KEYCODE_[B-Z] = [30-54] */
#define KEYCODE_COMMA         55
#define KEYCODE_PERIOD        56
#define KEYCODE_TAB           61
#define KEYCODE_SPACE         62
#define KEYCODE_ENTER         66
#define KEYCODE_DEL           67
#define KEYCODE_GRAVE         68
#define KEYCODE_MINUS         69
#define KEYCODE_EQUALS        70
#define KEYCODE_LEFT_BRACKET  71
#define KEYCODE_RIGHT_BRACKET 72
#define KEYCODE_BACKSLASH     73
#define KEYCODE_SEMICOLON     74
#define KEYCODE_APOSTROPHE    75
#define KEYCODE_SLASH         76
#define KEYCODE_AT            77
#define KEYCODE_PLUS          81
#define KEYCODE_PAGE_UP       92
#define KEYCODE_PAGE_DOWN     93
#define KEYCODE_ESCAPE        111
#define KEYCODE_FORWARD_DEL   112
#define KEYCODE_MOVE_HOME     122
#define KEYCODE_MOVE_END      123
#define KEYCODE_INSERT        124
#define KEYCODE_F1            131 /* KEYCODE_F[2-12] = [132-142] */
#define KEYCODE_NUMPAD_0      144 /* KEYCODE_NUMPAD_[1-9] = [145-153] */

static int map_key_code(int key_code)
{
	if (key_code >= GDK_KEY_0 && key_code <= GDK_KEY_9)
		return key_code - GDK_KEY_0 + KEYCODE_0;
	if (key_code >= GDK_KEY_a && key_code <= GDK_KEY_z)
		return key_code - GDK_KEY_a + KEYCODE_A;
	else if (key_code >= GDK_KEY_A && key_code <= GDK_KEY_Z)
		return key_code - GDK_KEY_A + KEYCODE_A;
	else if (key_code >= GDK_KEY_F1 && key_code <= GDK_KEY_F12)
		return key_code - GDK_KEY_F1 + KEYCODE_F1;
	else if (key_code >= GDK_KEY_KP_0 && key_code <= GDK_KEY_KP_9)
		return key_code - GDK_KEY_KP_0 + KEYCODE_NUMPAD_0;
	switch (key_code) {
		case GDK_KEY_Up:
			return KEYCODE_DPAD_UP;
		case GDK_KEY_Down:
			return KEYCODE_DPAD_DOWN;
		case GDK_KEY_Left:
			return KEYCODE_DPAD_LEFT;
		case GDK_KEY_Right:
			return KEYCODE_DPAD_RIGHT;
		case GDK_KEY_comma:
			return KEYCODE_COMMA;
		case GDK_KEY_period:
			return KEYCODE_PERIOD;
		case GDK_KEY_Tab:
			return KEYCODE_TAB;
		case GDK_KEY_space:
			return KEYCODE_SPACE;
		case GDK_KEY_Return:
			return KEYCODE_ENTER;
		case GDK_KEY_BackSpace:
			return KEYCODE_DEL;
		case GDK_KEY_grave:
			return KEYCODE_GRAVE;
		case GDK_KEY_minus:
			return KEYCODE_MINUS;
		case GDK_KEY_equal:
			return KEYCODE_EQUALS;
		case GDK_KEY_bracketleft:
			return KEYCODE_LEFT_BRACKET;
		case GDK_KEY_bracketright:
			return KEYCODE_RIGHT_BRACKET;
		case GDK_KEY_backslash:
			return KEYCODE_BACKSLASH;
		case GDK_KEY_semicolon:
			return KEYCODE_SEMICOLON;
		case GDK_KEY_apostrophe:
			return KEYCODE_APOSTROPHE;
		case GDK_KEY_slash:
			return KEYCODE_SLASH;
		case GDK_KEY_at:
			return KEYCODE_AT;
		case GDK_KEY_plus:
			return KEYCODE_PLUS;
		case GDK_KEY_Page_Up:
			return KEYCODE_PAGE_UP;
		case GDK_KEY_Page_Down:
			return KEYCODE_PAGE_DOWN;
		case GDK_KEY_Escape:
			return KEYCODE_ESCAPE;
		case GDK_KEY_Delete:
			return KEYCODE_FORWARD_DEL;
		case GDK_KEY_Home:
			return KEYCODE_MOVE_HOME;
		case GDK_KEY_End:
			return KEYCODE_MOVE_END;
		case GDK_KEY_Insert:
			return KEYCODE_INSERT;
		default:
			return key_code;
	}
}

#define META_SHIFT_ON     (1 << 0)
#define META_ALT_ON       (1 << 1)
#define META_CTRL_ON      (1 << 12)
#define META_META_ON      (1 << 16)
#define META_CAPS_LOCK_ON (1 << 20)

static int map_meta_state(GdkModifierType state)
{
	int meta_state = 0;
	if (state & GDK_SHIFT_MASK)
		meta_state |= META_SHIFT_ON;
	if (state & GDK_CONTROL_MASK)
		meta_state |= META_CTRL_ON;
	if (state & GDK_META_MASK)
		meta_state |= META_META_ON;
	if (state & GDK_ALT_MASK)
		meta_state |= META_ALT_ON;
	if (state & GDK_LOCK_MASK)
		meta_state |= META_CAPS_LOCK_ON;
	return meta_state;
}

static gboolean dispatch_key_event(ATLSceneWidget *scene, int action, guint keyval, GdkModifierType state)
{
	JNIEnv *env = get_jni_env();
	jobject key_event = (*env)->NewObject(env, handle_cache.key_event.class, handle_cache.key_event.constructor,
	                                      (jlong)0, (jlong)0, action, map_key_code(keyval), 0, map_meta_state(state));
	_SET_INT_FIELD(key_event, "unicodeValue", gdk_keyval_to_unicode(keyval));
	jboolean ret = (*env)->CallBooleanMethod(env, scene->view_root, scene->dispatch_key_event, key_event);
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionDescribe(env);
	(*env)->DeleteLocalRef(env, key_event);
	return ret;
}

static gboolean on_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, ATLSceneWidget *scene)
{
	return dispatch_key_event(scene, KEY_ACTION_DOWN, keyval, state);
}

static gboolean on_key_released(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, ATLSceneWidget *scene)
{
	return dispatch_key_event(scene, KEY_ACTION_UP, keyval, state);
}

static void atl_scene_widget_class_init(ATLSceneWidgetClass *class)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);
	widget_class->size_allocate = atl_scene_widget_size_allocate;
	widget_class->snapshot = atl_scene_widget_snapshot;
	G_OBJECT_CLASS(class)->dispose = atl_scene_widget_dispose;
}

GtkWidget *atl_scene_widget_new(JNIEnv *env, jobject view_root)
{
	ATLSceneWidget *scene = g_object_new(atl_scene_widget_get_type(), NULL);
	scene->view_root = (*env)->NewGlobalRef(env, view_root);
	jclass view_root_class = (*env)->GetObjectClass(env, view_root);
	scene->perform_layout = (*env)->GetMethodID(env, view_root_class, "performLayout", "(II)V");
	scene->perform_draw = (*env)->GetMethodID(env, view_root_class, "performDraw", "(JII)V");
	scene->dispatch_touch_event = (*env)->GetMethodID(env, view_root_class, "dispatchTouchEvent", "(Landroid/view/MotionEvent;)Z");
	scene->dispatch_key_event = (*env)->GetMethodID(env, view_root_class, "dispatchKeyEvent", "(Landroid/view/KeyEvent;)Z");
	(*env)->SetLongField(env, view_root, (*env)->GetFieldID(env, view_root_class, "scene", "J"), _INTPTR(scene));

	gtk_widget_set_hexpand(GTK_WIDGET(scene), TRUE);
	gtk_widget_set_vexpand(GTK_WIDGET(scene), TRUE);
	gtk_widget_set_focusable(GTK_WIDGET(scene), TRUE);

	GtkEventController *pointer_controller = gtk_event_controller_legacy_new();
	g_signal_connect(pointer_controller, "event", G_CALLBACK(on_pointer_event), scene);
	gtk_widget_add_controller(GTK_WIDGET(scene), pointer_controller);

	GtkEventController *key_controller = gtk_event_controller_key_new();
	g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_key_pressed), scene);
	g_signal_connect(key_controller, "key-released", G_CALLBACK(on_key_released), scene);
	gtk_widget_add_controller(GTK_WIDGET(scene), key_controller);

	return GTK_WIDGET(scene);
}

/* --- ViewRootImpl natives (invalidation may come from any thread) --- */

static guint queue_draw_cb(GtkWidget *widget)
{
	gtk_widget_queue_draw(widget);
	g_object_unref(widget);
	return G_SOURCE_REMOVE;
}

JNIEXPORT void JNICALL Java_android_view_ViewRootImpl_nativeInvalidate(JNIEnv *env, jclass class, jlong scene_ptr)
{
	g_idle_add_full(G_PRIORITY_HIGH_IDLE + 20, G_SOURCE_FUNC(queue_draw_cb), g_object_ref(GTK_WIDGET(_PTR(scene_ptr))), NULL);
}

static guint queue_allocate_cb(GtkWidget *widget)
{
	gtk_widget_queue_allocate(widget);
	gtk_widget_queue_draw(widget);
	g_object_unref(widget);
	return G_SOURCE_REMOVE;
}

JNIEXPORT void JNICALL Java_android_view_ViewRootImpl_nativeRequestLayout(JNIEnv *env, jclass class, jlong scene_ptr)
{
	g_idle_add_full(G_PRIORITY_HIGH_IDLE + 10, G_SOURCE_FUNC(queue_allocate_cb), g_object_ref(GTK_WIDGET(_PTR(scene_ptr))), NULL);
}
