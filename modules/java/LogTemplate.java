package org.syslog_ng;

public class LogTemplate {
  private long handle;
  private long cfg_handle;

  public LogTemplate(long cfg_handle) {
    this.cfg_handle = cfg_handle;
    handle = create_new_template_instance(cfg_handle);
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

  private native long create_new_template_instance(long cfg_handle);
  private native boolean compile(long handle, String template);
  private native String format(long handle, long msg_handle);
  private native void unref(long handle);
}
