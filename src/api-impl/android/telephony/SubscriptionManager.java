package android.telephony;

public class SubscriptionManager {

	public static final int INVALID_SUBSCRIPTION_ID = -1;

	/* there are no SIM subscriptions here, so there is no default data one */
	public static int getDefaultDataSubscriptionId() {
		return INVALID_SUBSCRIPTION_ID;
	}
}
