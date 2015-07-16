package org.syslog_ng.options;

public class IntegerOptionDecorator extends OptionDecorator {

	public IntegerOptionDecorator(Option decoratedOption) {
		super(decoratedOption);
	}

	public void validate() throws InvalidOptionException {
		decoratedOption.validate();
		try {
			Integer.parseInt(decoratedOption.getValue());
		}
		catch (NumberFormatException e) {
			throw new InvalidOptionException("option " + decoratedOption.getName() + " must be numerical");

		}
	}


}
