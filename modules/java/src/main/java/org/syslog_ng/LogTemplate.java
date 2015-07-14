package org.syslog_ng;

public class LogTemplate {
  private long configHandle;
  private long templateHandle;

  public static final int LTZ_LOCAL = 0;
  public static final int LTZ_SEND = 1;

  public LogTemplate(long configHandle) {
    this.configHandle = configHandle;
    templateHandle = create_new_template_instance(configHandle);
  }

  public boolean compile(String template) {
    return compile(templateHandle, template);
  }

  public String format(LogMessage msg) {
    return format(msg, 0, LTZ_SEND);
  }

  public String format(LogMessage msg, long logTemplateOptionsHandle) {
    return format(msg, logTemplateOptionsHandle, LTZ_SEND);
  }

  public String format(LogMessage msg, long logTemplateOptionsHandle, int timezone) {
    return format(templateHandle, msg.getHandle(), logTemplateOptionsHandle, timezone);
  }

  public void release() {
    unref(templateHandle);
  }

  private native long create_new_template_instance(long configHandle);
  private native boolean compile(long templateHandle, String template);
  private native String format(long templateHandle, long msgHandle, long templateOptionsHandle, int timezone);
  private native void unref(long templateHandle);
}
