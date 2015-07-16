package org.syslog_ng.options;

import java.util.Arrays;
import java.util.List;

public class BooleanOptionDecorator extends OptionDecorator {
    public static List ON_LIST = Arrays.asList("true", "on", "yes");
    public static List OFF_LIST = Arrays.asList("false", "off", "no");

    public BooleanOptionDecorator(Option decoratedOption) {
        super(decoratedOption);
    }

    public void validate() throws InvalidOptionException {
        decoratedOption.validate();

        Boolean value = getValueAsBoolean();

        if (value == null) {
            String errorMessage = String.format("invalid value of '%s': '%s'. Valid values are: 'true', 'on', 'yes', " +
                    "'false', 'off', 'no'", decoratedOption.getName(), value);
            throw new InvalidOptionException(errorMessage);
        }
    }

    public Boolean getValueAsBoolean() {
        String value = decoratedOption.getValue().toLowerCase();

        if (ON_LIST.contains(value)) {
            return true;
        } else if (OFF_LIST.contains(value)) {
            return false;
        } else {
            return null;
        }
    }
}
