package android.os;

import java.io.FileDescriptor;

public interface IBinder {

	public static final int FIRST_CALL_TRANSACTION = 0x00000001;
	public static final int LAST_CALL_TRANSACTION = 0x00ffffff;
	public static final int PING_TRANSACTION = ('_' << 24) | ('P' << 16) | ('N' << 8) | 'G';
	public static final int DUMP_TRANSACTION = ('_' << 24) | ('D' << 16) | ('M' << 8) | 'P';
	public static final int SHELL_COMMAND_TRANSACTION = ('_' << 24) | ('C' << 16) | ('M' << 8) | 'D';
	public static final int INTERFACE_TRANSACTION = ('_' << 24) | ('N' << 16) | ('T' << 8) | 'F';
	public static final int TWEET_TRANSACTION = ('_' << 24) | ('T' << 16) | ('W' << 8) | 'T';
	public static final int LIKE_TRANSACTION = ('_' << 24) | ('L' << 16) | ('I' << 8) | 'K';
	public static final int FLAG_ONEWAY = 0x00000001;

	public interface DeathRecipient {
		public void binderDied();
	}

	public String getInterfaceDescriptor() throws RemoteException;

	public boolean pingBinder();

	public boolean isBinderAlive();

	public IInterface queryLocalInterface(String descriptor);

	public void dump(FileDescriptor fd, String[] args) throws RemoteException;

	public void dumpAsync(FileDescriptor fd, String[] args) throws RemoteException;

	public boolean transact(int code, Parcel data, Parcel reply, int flags) throws RemoteException;

	public void linkToDeath(DeathRecipient recipient, int flags) throws RemoteException;

	public boolean unlinkToDeath(DeathRecipient recipient, int flags);
}
