package android.net;

import java.util.ArrayList;
import java.util.List;

/**
 * What a Network is carried by. ConnectivityManager fills this in from the
 * interface holding the default route; see getLinkProperties there.
 */
public class LinkProperties {

	private String interfaceName;
	private List<LinkAddress> linkAddresses = new ArrayList<LinkAddress>(0);

	public LinkProperties() {}

	LinkProperties(String interfaceName, List<LinkAddress> linkAddresses) {
		this.interfaceName = interfaceName;
		this.linkAddresses = linkAddresses;
	}

	public boolean isPrivateDnsActive() { return false; }

	public java.lang.String getDomains() { return null; }

	public java.lang.String getInterfaceName() { return interfaceName; }

	public java.util.List<LinkAddress> getLinkAddresses() { return linkAddresses; }
}
