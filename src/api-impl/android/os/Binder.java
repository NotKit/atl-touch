package android.os;

import android.content.Context;

import java.io.FileDescriptor;

public class Binder implements IBinder {

	private IInterface owner;
	private String descriptor;

	public Binder() {}

	public Binder(String descriptor) {
		this.descriptor = descriptor;
	}

	public void attachInterface(IInterface owner, String descriptor) {
		this.owner = owner;
		this.descriptor = descriptor;
	}

	public static void flushPendingCommands() {}

	public static long clearCallingIdentity() { return 0; }

	public static void restoreCallingIdentity(long identityToken) {}

	@Override
	public String getInterfaceDescriptor() { return descriptor; }

	@Override
	public boolean pingBinder() { return true; }

	@Override
	public boolean isBinderAlive() { return true; }

	@Override
	public IInterface queryLocalInterface(String descriptor) {
		return this.descriptor != null && this.descriptor.equals(descriptor) ? owner : null;
	}

	@Override
	public void dump(FileDescriptor fd, String[] args) {}

	@Override
	public void dumpAsync(FileDescriptor fd, String[] args) {}

	/**
	 * There is no binder driver under ATL, so a transaction is delivered in
	 * process: transact() calls straight through to onTransact().
	 */
	@Override
	public boolean transact(int code, Parcel data, Parcel reply, int flags) throws RemoteException {
		return onTransact(code, data, reply, flags);
	}

	protected boolean onTransact(int code, Parcel data, Parcel reply, int flags) throws RemoteException {
		return false;
	}

	@Override
	public void linkToDeath(DeathRecipient recipient, int flags) {}

	@Override
	public boolean unlinkToDeath(DeathRecipient recipient, int flags) { return true; }

	public static int getCallingUid() { return Context.this_application.getApplicationInfo().uid; }

	public static int getCallingPid() { return 0; }

	@Override
	public boolean equals(Object obj) {
		return obj instanceof Binder;
	}
}
