package android.net;

public class NetworkRequest {

	public class Builder {

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
