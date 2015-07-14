package org.syslog_ng;

public class LogMessage {
  private long handle;

  public LogMessage(long handle) {
    this.handle = handle;
  }

  public String getValue(String name) {
    return getValue(handle, name);
  }

  public void release() {
    unref(handle);
    handle = 0;
  }

  protected long getHandle() {
    return handle;
  }

  private native void unref(long handle);
  private native String getValue(long ptr, String name);
}
