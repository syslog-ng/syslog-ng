package org.syslog_ng.options;

import java.util.HashMap;

public class Options {
	private HashMap<String, Option> stringOptions;
	private HashMap<String, TemplateOption> templateOptions;

	public Options() {
		stringOptions = new HashMap<String, Option>();
		templateOptions = new HashMap<String, TemplateOption>();
	}

	public void put(Option stringOption) {
		stringOptions.put(stringOption.getName(), stringOption);
	}

	public void put(TemplateOption templateOption) {
		templateOptions.put(templateOption.getName(), templateOption);
	}

	public void validate() throws InvalidOptionException {
		validateStringOptions();
		validateTemplateOptions();
	}

	public void deinit() {
		for (String key : templateOptions.keySet()) {
			TemplateOption option = templateOptions.get(key);
			option.deinit();
		}
	}

	private void validateTemplateOptions() throws InvalidOptionException {
		for (String key : templateOptions.keySet()) {
			TemplateOption option = templateOptions.get(key);
			option.validate();
		}
	}

	public Option getStringOption(String name) {
		return stringOptions.get(name);
	}

	public TemplateOption getTemplateOption(String name) {
		return templateOptions.get(name);
	}

	public Option get(String name) {
		Option result = getStringOption(name);
		if (result == null) {
			result = getTemplateOption(name);
		}
		return result;
	}

	private void validateStringOptions() throws InvalidOptionException {
		for (String key : stringOptions.keySet()) {
			Option option = stringOptions.get(key);
			option.validate();
		}
	}
}
