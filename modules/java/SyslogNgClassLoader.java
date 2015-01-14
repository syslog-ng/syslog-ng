/*
 * Copyright (c) 2014 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2014 Viktor Juhasz <viktor.juhasz@balabit.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

package org.syslog_ng;

import java.io.File;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.lang.reflect.Method;


public class SyslogNgClassLoader {

  static {
    System.loadLibrary("mod-java");
  }

  private ClassLoader classLoader;

  public SyslogNgClassLoader() {
	  classLoader = ClassLoader.getSystemClassLoader();
  }

  public Class loadClass(String className, String pathList) {
    Class result = null;
	URL[] urls = createUrls(pathList);
	if (pathList != null) {
	  try {
	    expandClassPath(urls);
	  }
	  catch (Exception e) {
	    System.out.println("Error while expanding path list: " + e);
	    e.printStackTrace(System.err);
	  }
	}

	try {
	  result = Class.forName(className, true, classLoader);
	}
	catch (ClassNotFoundException e) {
	  System.out.println("Exception: " + e.getMessage());
	  e.printStackTrace(System.err);
	}
	catch (NoClassDefFoundError e) {
	  System.out.println("Error: " + e.getMessage());
	  e.printStackTrace(System.err);
	}
	return result;
  }

  private URL[] createUrls(String pathList) {
    String[] pathes = pathList.split(":");
    URL[] urls = new URL[pathes.length];
    int i = 0;
    for (String path:pathes) {
      try {
        urls[i++] = new File(path).toURI().toURL();
      }
      catch (MalformedURLException e) {
        e.printStackTrace();
      }
    }
    return urls;
  }

  private void expandClassPath(URL[] urls) throws Exception {
      Method method = URLClassLoader.class.getDeclaredMethod("addURL", new Class[]{URL.class});
      method.setAccessible(true);
      for (URL url:urls) {
          method.invoke(classLoader, new Object[]{url});
      }
  }
}
