package android.media;

public class MediaDrm {
	public static final int EVENT_KEY_EXPIRED = 3;
	public static final int EVENT_KEY_REQUIRED = 2;
	public static final int EVENT_PROVISION_REQUIRED = 1;
	public static final int EVENT_SESSION_RECLAIMED = 5;
	public static final int EVENT_VENDOR_DEFINED = 4;
	public static final int HDCP_LEVEL_UNKNOWN = 0;
	public static final int HDCP_NONE = 1;
	public static final int HDCP_NO_DIGITAL_OUTPUT = 2147483647;
	public static final int HDCP_V1 = 2;
	public static final int HDCP_V2 = 3;
	public static final int HDCP_V2_1 = 4;
	public static final int HDCP_V2_2 = 5;
	public static final int HDCP_V2_3 = 6;
	public static final int KEY_TYPE_OFFLINE = 2;
	public static final int KEY_TYPE_RELEASE = 3;
	public static final int KEY_TYPE_STREAMING = 1;
	public static final int OFFLINE_LICENSE_STATE_RELEASED = 2;
	public static final int OFFLINE_LICENSE_STATE_UNKNOWN = 0;
	public static final int OFFLINE_LICENSE_STATE_USABLE = 1;
	public static final String PROPERTY_ALGORITHMS = "algorithms";
	public static final String PROPERTY_DESCRIPTION = "description";
	public static final String PROPERTY_DEVICE_UNIQUE_ID = "deviceUniqueId";
	public static final String PROPERTY_VENDOR = "vendor";
	public static final String PROPERTY_VERSION = "version";
	public static final int SECURITY_LEVEL_HW_SECURE_ALL = 5;
	public static final int SECURITY_LEVEL_HW_SECURE_CRYPTO = 3;
	public static final int SECURITY_LEVEL_HW_SECURE_DECODE = 4;
	public static final int SECURITY_LEVEL_SW_SECURE_CRYPTO = 1;
	public static final int SECURITY_LEVEL_SW_SECURE_DECODE = 2;
	public static final int SECURITY_LEVEL_UNKNOWN = 0;

	public static final class KeyRequest {
		public static final int REQUEST_TYPE_INITIAL = 0;
		public static final int REQUEST_TYPE_NONE = 3;
		public static final int REQUEST_TYPE_RELEASE = 2;
		public static final int REQUEST_TYPE_RENEWAL = 1;
		public static final int REQUEST_TYPE_UPDATE = 4;

		public byte[] getData() { return null; }

		public String getDefaultUrl() { return null; }

		public int getRequestType() { return REQUEST_TYPE_NONE; }
	}

	public interface OnEventListener {
	
	public default void onEvent(android.media.MediaDrm a0, byte[] a1, int a2, int a3, byte[] a4) { }
}

	public interface OnKeyStatusChangeListener {
	
	public default void onKeyStatusChange(android.media.MediaDrm a0, byte[] a1, java.util.List a2, boolean a3) { }
}

	public android.media.MediaDrm.KeyRequest getKeyRequest(byte[] a0, byte[] a1, java.lang.String a2, int a3, java.util.HashMap a4) throws android.media.NotProvisionedException { return null; }

	public android.media.MediaDrm.ProvisionRequest getProvisionRequest() { return null; }

	public byte[] openSession() throws android.media.NotProvisionedException, android.media.ResourceBusyException { return null; }

	public byte[] provideKeyResponse(byte[] a0, byte[] a1) throws android.media.DeniedByServerException, android.media.NotProvisionedException { return null; }

	public java.util.HashMap queryKeyStatus(byte[] a0) { return null; }

	public static boolean isCryptoSchemeSupported(java.util.UUID a0) { return false; }

	public static boolean isCryptoSchemeSupported(java.util.UUID a0, java.lang.String a1) { return false; }

	public static class KeyStatus { 
	public byte[] getKeyId() { return null; }

	public int getStatusCode() { return 0; }
}

	public static class ProvisionRequest { 
	public byte[] getData() { return null; }

	public java.lang.String getDefaultUrl() { return null; }
}

	public void closeSession(byte[] a0) { }

	public void provideProvisionResponse(byte[] a0) throws android.media.DeniedByServerException { }

	public void release() { }

	public void setOnEventListener(android.media.MediaDrm.OnEventListener a0) { }

	public void setOnKeyStatusChangeListener(android.media.MediaDrm.OnKeyStatusChangeListener a0, android.os.Handler a1) { }

	public void setOnKeyStatusChangeListener(java.util.concurrent.Executor a0, android.media.MediaDrm.OnKeyStatusChangeListener a1) { }

	public void setPropertyByteArray(java.lang.String a0, byte[] a1) { }

	public void setPropertyString(java.lang.String a0, java.lang.String a1) { }
}
