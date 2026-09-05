package android.net;

public class LinkProperties {

	public boolean isPrivateDnsActive() { return false; }

	public java.lang.String getDomains() { return null; }

	/* the interface behind a Network is not tracked, so it is unknown */
	public java.lang.String getInterfaceName() { return null; }

	/* and no per-interface addresses are collected either */
	public java.util.List<LinkAddress> getLinkAddresses() {
		return new java.util.ArrayList<LinkAddress>(0);
	}
}
