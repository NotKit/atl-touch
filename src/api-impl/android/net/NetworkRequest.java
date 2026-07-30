package android.net;

public class NetworkRequest {

	/* static: apps build a request before they have one */
	public static class Builder {

		public NetworkRequest build() {
			return new NetworkRequest();
		}

		public Builder addCapability(int capability) {
			return this;
		}

		public Builder addTransportType(int transportType) {
			return this;
		}
	}
}
