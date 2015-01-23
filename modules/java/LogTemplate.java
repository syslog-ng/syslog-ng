package org.syslog_ng;

public class LogTemplate {
  private long handle;
  private long configHandle;

  public LogTemplate(long configHandle) {
    this.configHandle = configHandle;
    handle = create_new_template_instance(configHandle);
  }

  public boolean compile(String template) {
    return compile(handle, template);
  }

  public String format(LogMessage msg) {
    return format(handle, msg.getHandle());
  }

  public void release() {
	unref(handle);
  }

  private native long create_new_template_instance(long configHandle);
  private native boolean compile(long handle, String template);
  private native String format(long handle, long msg_handle);
  private native void unref(long handle);
}
