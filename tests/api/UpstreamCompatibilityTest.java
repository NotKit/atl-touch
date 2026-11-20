import android.content.ContentProvider;
import android.content.ContentProviderOperation;
import android.content.ContentProviderResult;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.NetworkCapabilities;
import android.net.Uri;
import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.Parcel;
import android.os.ParcelFileDescriptor;
import android.util.AtomicFile;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;

public class UpstreamCompatibilityTest {
    private static void check(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }

    private static void parcels() throws Exception {
        Parcel empty = Parcel.obtain();
        check(empty.readInt() == 0, "empty parcel int");
        check(empty.readString() == null, "empty parcel string");
        empty.readException();

        final Binder receiver = new Binder() {
            @Override
            protected boolean onTransact(int code, Parcel data, Parcel reply, int flags) {
                Message message = Message.CREATOR.createFromParcel(data);
                check(message.what == 42 && message.arg1 == 7, "message fields");
                check("payload".equals(message.getData().getString("key")), "message bundle");
                reply.writeNoException();
                return true;
            }
        };
        Message message = Message.obtain();
        message.what = 42;
        message.arg1 = 7;
        Bundle bundle = new Bundle();
        bundle.putString("key", "payload");
        message.setData(bundle);
        new Messenger(receiver).send(message);

        Parcel parcel = Parcel.obtain();
        parcel.writeStrongBinder(receiver);
        parcel.writeStrongBinder(null);
        parcel.setDataPosition(0);
        check(parcel.readStrongBinder() == receiver, "binder identity");
        check(parcel.readStrongBinder() == null, "null binder");

        Uri uri = Uri.parse("content://example/path?q=value#fragment");
        parcel = Parcel.obtain();
        uri.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);
        check(uri.equals(Uri.CREATOR.createFromParcel(parcel)), "URI parcel");

        parcel = Parcel.obtain();
        bundle.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);
        check(Bundle.CREATOR != null, "Bundle creator");
        check("payload".equals(parcel.readBundle().getString("key")), "Bundle parcel");
    }

    private static void batches() throws Exception {
        ContentProvider provider = new ContentProvider() {
            @Override public Cursor query(Uri uri, String[] projection, String selection,
                                          String[] args, String order) { return null; }
            @Override public Uri insert(Uri uri, ContentValues values) { return uri; }
            @Override public int update(Uri uri, ContentValues values, String selection,
                                        String[] args) {
                check("saved".equals(values.getAsString("name")), "batch value");
                return 1;
            }
            @Override public int delete(Uri uri, String selection, String[] args) { return 1; }
            @Override public String getType(Uri uri) { return "text/plain"; }
            @Override public ParcelFileDescriptor openFile(Uri uri, String mode) { return null; }
        };
        ContentProviderOperation operation = ContentProviderOperation.newUpdate(
            Uri.parse("content://example/row/1")).withValue("name", "saved")
            .withExpectedCount(1).withYieldAllowed(true).build();
        Parcel parcel = Parcel.obtain();
        operation.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);
        operation = ContentProviderOperation.CREATOR.createFromParcel(parcel);
        check(operation.isUpdate() && operation.isYieldAllowed(), "batch flags");
        ArrayList<ContentProviderOperation> operations = new ArrayList<>();
        operations.add(operation);
        ContentProviderResult[] results = provider.applyBatch("example", operations);
        check(results.length == 1 && results[0].count == 1, "batch result");
    }

    private static void atomicFiles() throws Exception {
        File directory = Files.createTempDirectory("atl-atomic-").toFile();
        AtomicFile file = new AtomicFile(new File(directory, "state"));
        try {
            FileOutputStream output = file.startWrite();
            output.write("saved".getBytes(StandardCharsets.UTF_8));
            file.finishWrite(output);
            output = file.startWrite();
            output.write("interrupted".getBytes(StandardCharsets.UTF_8));
            file.failWrite(output);
            check("saved".equals(new String(file.readFully(), StandardCharsets.UTF_8)),
                  "failed write preserves previous data");
        } finally {
            file.delete();
            check(directory.delete(), "temporary directory cleanup");
        }
    }

    public static void main(String[] args) throws Exception {
        parcels();
        batches();
        atomicFiles();
        NetworkCapabilities caps = new NetworkCapabilities()
            .addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
            .addTransportType(NetworkCapabilities.TRANSPORT_WIFI);
        check(caps.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET), "capability");
        check(caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI), "transport");
        check(!caps.hasCapability(-1), "invalid capability");
        System.out.println("upstream compatibility: passed");
    }
}
