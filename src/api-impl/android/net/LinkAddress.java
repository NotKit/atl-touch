package android.net;

import java.net.InetAddress;

/**
 * An address assigned to a network interface. Nothing here hands one out --
 * LinkProperties has no addresses to report -- so this only gives callers
 * walking that list a type to name.
 */
public class LinkAddress {

	private final InetAddress address;
	private final int prefixLength;

	public LinkAddress(InetAddress address, int prefixLength) {
		this.address = address;
		this.prefixLength = prefixLength;
	}

	public InetAddress getAddress() {
		return address;
	}

	public int getPrefixLength() {
		return prefixLength;
	}

	public String toString() {
		return (address == null ? "" : address.getHostAddress()) + "/" + prefixLength;
	}
}
