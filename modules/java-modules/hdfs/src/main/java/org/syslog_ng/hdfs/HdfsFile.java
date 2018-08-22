/*
 * Copyright (c) 2017 Balabit
 * Copyright (c) 2017 Zoltan Pallagi <zoltan.pallagi@balabit.com>
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

package org.syslog_ng.hdfs;

import org.apache.hadoop.fs.FSDataOutputStream;
import org.apache.hadoop.fs.Path;

import java.io.IOException;

public class HdfsFile {
    private FSDataOutputStream fsDataOutputStream;
    private Path path;

    public FSDataOutputStream getFsDataOutputStream() {
        return fsDataOutputStream;
    }

    public void setFsDataOutputStream(FSDataOutputStream fsDataOutputStream) {
        this.fsDataOutputStream = fsDataOutputStream;
    }

    public Path getPath() {
        return path;
    }

    public void setPath(Path path) {
        this.path = path;
    }

    public void flush() throws IOException {
         if (fsDataOutputStream == null)
            return;

         fsDataOutputStream.hflush();
         fsDataOutputStream.hsync();
    }

    public void close() throws IOException {
         if (fsDataOutputStream == null)
            return;

          fsDataOutputStream.close();
          fsDataOutputStream = null;
    }
}
