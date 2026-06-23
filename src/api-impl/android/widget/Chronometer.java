/*
 * Copyright (C) 2006 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.os.Handler;
import android.os.Message;
import android.os.SystemClock;
import android.text.format.DateUtils;
import android.util.AttributeSet;

import java.util.Formatter;
import java.util.IllegalFormatException;
import java.util.Locale;

public class Chronometer extends TextView {

	public interface OnChronometerTickListener {
		void onChronometerTick(Chronometer chronometer);
	}

	private static final int TICK_WHAT = 2;

	private long mBase;
	private long mNow;
	private boolean mVisible;
	private boolean mStarted;
	private boolean mRunning;
	private boolean mLogged;
	private String mFormat;
	private Formatter mFormatter;
	private Locale mFormatterLocale;
	private Object[] mFormatterArgs = new Object[1];
	private StringBuilder mFormatBuilder;
	private OnChronometerTickListener mOnChronometerTickListener;
	private StringBuilder mRecycle = new StringBuilder(8);
	private boolean mCountDown;

	public Chronometer(Context context) {
		this(context, null, 0);
	}

	public Chronometer(Context context, AttributeSet attrs) {
		this(context, attrs, 0);
	}

	public Chronometer(Context context, AttributeSet attrs, int defStyleAttr) {
		this(context, attrs, defStyleAttr, 0);
	}

	public Chronometer(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
		super(context, attrs, defStyleAttr, defStyleRes);

		TypedArray a = context.obtainStyledAttributes(attrs, com.android.internal.R.styleable.Chronometer, defStyleAttr, defStyleRes);
		setFormat(a.getString(com.android.internal.R.styleable.Chronometer_format));
		setCountDown(a.getBoolean(com.android.internal.R.styleable.Chronometer_countDown, false));
		a.recycle();

		init();
	}

	private void init() {
		mBase = SystemClock.elapsedRealtime();
		updateText(mBase);
	}

	public void setCountDown(boolean countDown) {
		mCountDown = countDown;
		updateText(SystemClock.elapsedRealtime());
	}

	public boolean isCountDown() {
		return mCountDown;
	}

	public boolean isTheFinalCountDown() {
		return false;
	}

	public void setBase(long base) {
		mBase = base;
		updateText(SystemClock.elapsedRealtime());
	}

	public long getBase() {
		return mBase;
	}

	public void setFormat(String format) {
		mFormat = format;
		if (format != null && mFormatBuilder == null) {
			mFormatBuilder = new StringBuilder(format.length() * 2);
		}
	}

	public String getFormat() {
		return mFormat;
	}

	public void setOnChronometerTickListener(OnChronometerTickListener listener) {
		mOnChronometerTickListener = listener;
	}

	public OnChronometerTickListener getOnChronometerTickListener() {
		return mOnChronometerTickListener;
	}

	public void start() {
		mStarted = true;
		updateRunning();
	}

	public void stop() {
		mStarted = false;
		updateRunning();
	}

	public void setStarted(boolean started) {
		mStarted = started;
		updateRunning();
	}

	@Override
	protected void onAttachedToWindow() {
		super.onAttachedToWindow();
		mVisible = true;
		updateRunning();
	}

	@Override
	protected void onDetachedFromWindow() {
		super.onDetachedFromWindow();
		mVisible = false;
		updateRunning();
	}

	private synchronized void updateText(long now) {
		mNow = now;
		long seconds = mCountDown ? mBase - now : now - mBase;
		seconds /= 1000;
		boolean negative = false;
		if (seconds < 0) {
			seconds = -seconds;
			negative = true;
		}

		String text = DateUtils.formatElapsedTime(seconds);
		if (negative) {
			text = getResources().getString(com.android.internal.R.string.negative_duration, text);
		}

		if (mFormat != null) {
			Locale loc = Locale.getDefault();
			if (mFormatter == null || !loc.equals(mFormatterLocale)) {
				mFormatterLocale = loc;
				mFormatter = new Formatter(mFormatBuilder, loc);
			}
			mFormatBuilder.setLength(0);
			mFormatterArgs[0] = text;
			try {
				mFormatter.format(mFormat, mFormatterArgs);
				text = mFormatBuilder.toString();
			} catch (IllegalFormatException ex) {
				if (!mLogged) {
					mLogged = true;
				}
			}
		}
		setText(text);
	}

	private void updateRunning() {
		boolean running = mVisible && mStarted && isShown();
		if (running != mRunning) {
			if (running) {
				updateText(SystemClock.elapsedRealtime());
				dispatchChronometerTick();
				mHandler.sendMessageDelayed(Message.obtain(mHandler, TICK_WHAT),
						1000 - (SystemClock.elapsedRealtime() - mBase) % 1000);
			} else {
				mHandler.removeMessages(TICK_WHAT);
			}
			mRunning = running;
		}
	}

	private final Handler mHandler = new Handler() {
		@Override
		public void handleMessage(Message m) {
			if (mRunning) {
				updateText(SystemClock.elapsedRealtime());
				dispatchChronometerTick();
				sendMessageDelayed(Message.obtain(this, TICK_WHAT),
						1000 - (SystemClock.elapsedRealtime() - mBase) % 1000);
			}
		}
	};

	void dispatchChronometerTick() {
		if (mOnChronometerTickListener != null) {
			mOnChronometerTickListener.onChronometerTick(this);
		}
	}
}
