/*
 * Copyright (C) 2014 The Android Open Source Project
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

import android.content.res.Resources;

/**
 * An interface that can be used on any {@link android.widget.Adapter Adapter}
 * to provide a themed {@link Context} for the views in the drop down popup.
 */
public interface ThemedSpinnerAdapter extends SpinnerAdapter {
	/**
	 * Sets the {@link Resources.Theme} against which drop-down views are
	 * inflated.
	 *
	 * @param theme the context against which to inflate drop-down views, or
	 *              {@code null} to use the default theme
	 */
	void setDropDownViewTheme(Resources.Theme theme);

	/**
	 * Returns the value previously set by a call to
	 * {@link #setDropDownViewTheme(Resources.Theme)}.
	 *
	 * @return the {@link Resources.Theme} against which drop-down views are
	 *         inflated, or {@code null} if one has not been set
	 */
	Resources.Theme getDropDownViewTheme();
}
