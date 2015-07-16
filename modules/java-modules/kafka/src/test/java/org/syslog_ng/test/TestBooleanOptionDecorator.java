package org.syslog_ng.test;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.syslog_ng.options.BooleanOptionDecorator;
import org.syslog_ng.options.StringOption;

import java.util.Arrays;

public class TestBooleanOptionDecorator extends TestOption {

    @Before
    public void setUp() throws Exception {
        super.setUp();
    }

    @After
    public void tearDown() {
        options.clear();
    }

    @Test
    public void testTrueValues() {
        for (String value: Arrays.asList("true", "on", "yes", "Yes", "YES")) {
            options.put("key", value);
            StringOption option = new StringOption(owner, "key");
            BooleanOptionDecorator decorator = new BooleanOptionDecorator(option);
            assertInitOptionSuccess(decorator);
            Assert.assertEquals(decorator.getValueAsBoolean(), Boolean.TRUE);
        }
    }

    @Test
    public void testFalseValues() {
        for (String value: Arrays.asList("false", "off", "no", "NO", "OFF")) {
            options.put("key", value);
            StringOption option = new StringOption(owner, "key");
            BooleanOptionDecorator decorator = new BooleanOptionDecorator(option);
            assertInitOptionSuccess(decorator);
            Assert.assertEquals(decorator.getValueAsBoolean(), Boolean.FALSE);
        }
    }

    @Test
    public void testFailingValues() {
        for (String value: Arrays.asList("alpha", "beta")) {
            options.put("key", value);
            StringOption option = new StringOption(owner, "key");
            BooleanOptionDecorator decorator = new BooleanOptionDecorator(option);
            assertInitOptionFailed(decorator);
            Assert.assertEquals(decorator.getValueAsBoolean(), null);
        }
    }
}