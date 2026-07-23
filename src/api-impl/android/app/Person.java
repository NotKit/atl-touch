package android.app;

import android.graphics.drawable.Icon;

public class Person {

	private CharSequence name;
	private Icon icon;
	private String uri;
	private String key;
	private boolean bot;
	private boolean important;

	private Person() {}

	public CharSequence getName() { return name; }

	public Icon getIcon() { return icon; }

	public String getUri() { return uri; }

	public String getKey() { return key; }

	public boolean isBot() { return bot; }

	public boolean isImportant() { return important; }

	public Builder toBuilder() {
		return new Builder()
			.setName(name)
			.setIcon(icon)
			.setUri(uri)
			.setKey(key)
			.setBot(bot)
			.setImportant(important);
	}

	public static class Builder {
		private final Person person = new Person();

		public Builder setName(CharSequence name) { person.name = name; return this; }

		public Builder setIcon(Icon icon) { person.icon = icon; return this; }

		public Builder setUri(String uri) { person.uri = uri; return this; }

		public Builder setKey(String key) { person.key = key; return this; }

		public Builder setBot(boolean bot) { person.bot = bot; return this; }

		public Builder setImportant(boolean important) { person.important = important; return this; }

		public Person build() { return person; }
	}
}
