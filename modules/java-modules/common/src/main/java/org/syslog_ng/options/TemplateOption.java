package org.syslog_ng.options;

import org.syslog_ng.LogMessage;
import org.syslog_ng.LogTemplate;

public class TemplateOption extends OptionDecorator {
	private LogTemplate template;
	private String strTemplate;
	private long configHandle;

	public TemplateOption(long configHandle, Option decoratedOption) {
		super(decoratedOption);
		this.configHandle = configHandle;
	}

	@Override
	public void validate() throws InvalidOptionException {
		decoratedOption.validate();
		strTemplate = decoratedOption.getValue();
		if (strTemplate != null) {
			template = new LogTemplate(configHandle);
			if(!template.compile(strTemplate)) {
				throw new InvalidOptionException("Can't compile template: '" + strTemplate + "'");
			}
		}
	}

	public void deinit() {
		if (template != null)
			template.release();
	}

	public String getResolvedString(LogMessage msg) {
		if (template != null)
			return template.format(msg);
		return null;
	}

	public String getResolvedString(LogMessage msg, long templateOptionsHandle, int timezone) {
		if (template != null)
			return template.format(msg, templateOptionsHandle, timezone);
		return null;
	}

	public String getResolvedString(LogMessage msg, long templateOptionsHandle) {
		if (template != null)
			return template.format(msg, templateOptionsHandle);
		return null;
	}
}
