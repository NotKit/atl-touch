package android.os;

public abstract class CountDownTimer {
	private static final int MSG = 1;

	private final long millisInFuture;
	private final long countDownInterval;
	private long stopTimeInFuture;
	private boolean cancelled;

	public CountDownTimer(long millisInFuture, long countDownInterval) {
		this.millisInFuture = millisInFuture;
		this.countDownInterval = countDownInterval;
	}

	public final synchronized void cancel() {
		cancelled = true;
		handler.removeMessages(MSG);
	}

	public abstract void onFinish();
	public abstract void onTick(long millisUntilFinished);

	public final synchronized CountDownTimer start() {
		cancelled = false;
		handler.removeMessages(MSG);
		if (millisInFuture <= 0) {
			onFinish();
			return this;
		}
		stopTimeInFuture = SystemClock.elapsedRealtime() + millisInFuture;
		handler.sendMessage(handler.obtainMessage(MSG));
		return this;
	}

	private final Handler handler = new Handler() {
		@Override
		public void handleMessage(Message message) {
			synchronized (CountDownTimer.this) {
				if (cancelled) return;

				long millisLeft = stopTimeInFuture - SystemClock.elapsedRealtime();
				if (millisLeft <= 0) {
					onFinish();
					return;
				}

				long delay;
				if (millisLeft < countDownInterval) {
					delay = millisLeft;
				} else {
					long tickStart = SystemClock.elapsedRealtime();
					onTick(millisLeft);
					delay = countDownInterval - (SystemClock.elapsedRealtime() - tickStart);
					while (delay < 0) delay += countDownInterval;
				}
				handler.sendMessageDelayed(handler.obtainMessage(MSG), delay);
			}
		}
	};
}
