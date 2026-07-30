package android.atl;

import java.io.FileDescriptor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.net.DatagramSocket;
import java.net.Socket;

/**
 * Reaches the int inside a FileDescriptor.
 *
 * libcore adds getInt$/setInt$ to FileDescriptor and getFileDescriptor$ to the
 * socket classes; a stock JDK has neither, and framework code that calls them
 * directly only links on ART. Go through the libcore methods when the runtime
 * has them and fall back to the private field otherwise (which needs
 * --add-opens java.base/java.io=ALL-UNNAMED on the JVM command line).
 */
public final class FileDescriptorUtils {
	private static final Method GET_INT = method(FileDescriptor.class, "getInt$");
	private static final Method SET_INT = method(FileDescriptor.class, "setInt$", int.class);
	private static final Field FD_FIELD = field(FileDescriptor.class, "fd");

	private FileDescriptorUtils() {}

	public static int getInt(FileDescriptor fd) {
		try {
			if (GET_INT != null)
				return (Integer)GET_INT.invoke(fd);
			if (FD_FIELD != null)
				return FD_FIELD.getInt(fd);
		} catch (ReflectiveOperationException e) {
			throw new UnsupportedOperationException("cannot read a FileDescriptor", e);
		}
		throw new UnsupportedOperationException("cannot read a FileDescriptor on this runtime");
	}

	public static void setInt(FileDescriptor fd, int value) {
		try {
			if (SET_INT != null) {
				SET_INT.invoke(fd, value);
				return;
			}
			if (FD_FIELD != null) {
				FD_FIELD.setInt(fd, value);
				return;
			}
		} catch (ReflectiveOperationException e) {
			throw new UnsupportedOperationException("cannot set a FileDescriptor", e);
		}
		throw new UnsupportedOperationException("cannot set a FileDescriptor on this runtime");
	}

	public static FileDescriptor of(Socket socket) {
		return descriptorOf(socket, "getFileDescriptor$");
	}

	public static FileDescriptor of(DatagramSocket socket) {
		return descriptorOf(socket, "getFileDescriptor$");
	}

	private static FileDescriptor descriptorOf(Object socket, String name) {
		Method method = method(socket.getClass(), name);
		if (method == null)
			throw new UnsupportedOperationException(socket.getClass().getName()
			    + " does not expose its file descriptor on this runtime");
		try {
			return (FileDescriptor)method.invoke(socket);
		} catch (ReflectiveOperationException e) {
			throw new UnsupportedOperationException("cannot read the socket descriptor", e);
		}
	}

	private static Method method(Class<?> cls, String name, Class<?>... parameters) {
		try {
			Method method = cls.getMethod(name, parameters);
			method.setAccessible(true);
			return method;
		} catch (ReflectiveOperationException | RuntimeException e) {
			return null;
		}
	}

	private static Field field(Class<?> cls, String name) {
		try {
			Field field = cls.getDeclaredField(name);
			field.setAccessible(true);
			return field;
		} catch (ReflectiveOperationException | RuntimeException e) {
			return null;
		}
	}
}
