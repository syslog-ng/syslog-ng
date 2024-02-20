/*
 * Copyright (c) 2014 Balabit
 * Copyright (c) 2014 Viktor Juhasz <viktor.juhasz@balabit.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

package org.syslog_ng;

import java.io.File;
import java.io.FilenameFilter;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.file.FileSystems;
import java.nio.file.Path;
import java.nio.file.PathMatcher;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;
import java.lang.reflect.Method;
import java.lang.ClassLoader;


public class SyslogNgClassLoader extends URLClassLoader {

  static {
    System.loadLibrary("mod-java");
  }

  public SyslogNgClassLoader() {
    super(new URL[]{});
  }

  public SyslogNgClassLoader(ClassLoader parent) {
    super(new URL[]{}, parent);
  }

  public static void initCurrentThread() {
    java.lang.Thread.currentThread().setContextClassLoader(ClassLoader.getSystemClassLoader());
  }

  public static Class loadClass(String className, String pathList) {
    Class result = null;
    URL[] urls = createUrls(pathList);

    try {
      SyslogNgClassLoader systemClassLoader = (SyslogNgClassLoader) ClassLoader.getSystemClassLoader();

      if (urls.length > 0) {
        systemClassLoader.expandClassPath(urls);
      }

      result = Class.forName(className, true, systemClassLoader);
    }
    catch (ClassNotFoundException e) {
      InternalMessageSender.error("Exception: " + e.getMessage());
      e.printStackTrace(System.err);
    }
    catch (NoClassDefFoundError e) {
      InternalMessageSender.error("Error: " + e.getMessage());
      e.printStackTrace(System.err);
    }
    catch (Exception e) {
      InternalMessageSender.error("Error while expanding path list: " + e.getMessage());
      e.printStackTrace(System.err);
    }

    return result;
  }

  private static List<String> resolveWildCardFileName(String path) {
    List<String> result = new ArrayList<String>();
    File f = new File(path);
    final String basename = f.getName();
    File dir;
    try {
      String dirname = f.getParent();
      dir = new File(dirname);
    }
    catch (NullPointerException e) {
      InternalMessageSender.error("Error. Null pointer dereference. Path or pattern might not exist:" + path + ". Error:" + e.getMessage());
      e.printStackTrace(System.err);
      return result;
    }
    File[] files = dir.listFiles(new FilenameFilter() {

      public boolean accept(File dir, String name) {
        PathMatcher matcher = FileSystems.getDefault().getPathMatcher("glob:"+basename);
        Path path = Paths.get(name);
        return matcher.matches(path);
      }
    });
    if (files != null) {
      for (File found:files) {
        result.add(found.getAbsolutePath());
      }
    }
    return result;
  }

  private static String[] parsePathList(String pathList) {
    String[] pathes = pathList.split(":");
    List<String> result = new ArrayList<String>();

    for (String path:pathes) {
      if (path.indexOf('*') != -1) {
        result.addAll(resolveWildCardFileName(path));
      }
      else {
        result.add(path);
      }
    }

    return result.toArray(new String[result.size()]);
  }


  private static URL[] createUrls(String pathList) {
    String[] pathes = parsePathList(pathList);
    URL[] urls = new URL[pathes.length];
    int i = 0;
    for (String path:pathes) {
      try {
        InternalMessageSender.debug("Add path to classpath: " + path);
        urls[i++] = new File(path).toURI().toURL();
      }
      catch (MalformedURLException e) {
        e.printStackTrace();
      }
    }
    return urls;
  }

  public void expandClassPath(URL[] urls) throws Exception {
      for (URL url : urls) {
          this.addURL(url);
      }
  }
}
