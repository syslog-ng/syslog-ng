/*
 * Copyright (c) 2015 BalaBit
 * All Rights Reserved.
 * Authors: Zoltan Pallagi <zoltan.pallagi@balabit.com>
 */

package org.syslog_ng.hdfs;

import org.syslog_ng.*;
import org.syslog_ng.options.InvalidOptionException;
import org.syslog_ng.logging.SyslogNgInternalLogger;

import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.FSDataOutputStream;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.Path;
import org.apache.log4j.Level;
import org.apache.log4j.Logger;

import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import java.nio.charset.Charset;
import java.util.Locale;
import java.util.UUID;

public class HdfsDestination extends TextLogDestination {
    private static final String LOG_TAG = "HDFS:";

    HdfsOptions options;
    Logger logger;

    private FileSystem hdfs;
    private FSDataOutputStream fsDataOutputStream;

    private Path currentFilePath;

    private boolean isOpened;
    private int maxFileNameLength = 255;

    public HdfsDestination(long handle) {
        super(handle);
        logger = Logger.getRootLogger();
        SyslogNgInternalLogger.register(logger);
        options = new HdfsOptions(this);
    }

    @Override
    public boolean init() {
        try {
          options.init();
          logger.debug("Initialize hdfs destination");
        } catch(InvalidOptionException e) {
          return false;
        }
        return true;
    }

    @Override
    public boolean isOpened() {
        return isOpened;
    }

    @Override
    public boolean open() {
        logger.debug("Opening hdfs");
        isOpened = false;
        try {
            Configuration configuration = new Configuration();
            loadConfigResources(configuration);

            hdfs = FileSystem.get(new URI(options.getUri()), configuration);
            hdfs.getStatus(); // Just to be sure we are really connected to hdfs
            isOpened = true;
        } catch (Exception e) {
            printStackTrace(e);
            closeAll(false);
        }
        return isOpened;
    }

    private void loadConfigResources(Configuration configuration) {
        String[] resources = options.getResouces();
        if (resources == null) {
          return;
        }
        for (int i = 0; i < resources.length; i++) {
            String resource = resources[i];
            configuration.addResource(new Path(resource));
            logger.debug(String.format("Resource loaded: %s", resource));
        }
    }

    @Override
    public boolean send(String message) {
        isOpened = false;

        if (!ensureDataOutputStream()) {
            return false;
        }

        try {
            logger.debug("Outgoing message: " + message);
            fsDataOutputStream.write(message.getBytes(Charset.forName("UTF-8")));
        } catch (IOException e) {
            printStackTrace(e);
            closeAll(false);
            return false;
        }

        isOpened = true;
        return true;
    }

    private String truncateFileName(String fileName, int maxLength) {
        if (fileName.length() > maxLength) {
            return fileName.substring(0, maxLength);
        }
        return fileName;
    }

    private boolean ensureDataOutputStream() {
        if (fsDataOutputStream == null) {
            // file not yet opened
            String currentFile = String.format("%s.%s", options.getFile(), UUID.randomUUID());
            currentFilePath = new Path(String.format("%s/%s", options.getUri(), currentFile));

            if (currentFilePath.getName().length() > options.getMaxFilenameLength()) {
                String fileName = truncateFileName(currentFilePath.getName(), options.getMaxFilenameLength());
                logger.debug(String.format("Maximum file name length (%s) exceeded, truncated to %s", maxFileNameLength, fileName));

                currentFilePath = new Path(currentFilePath.getParent(), fileName);
            }

            fsDataOutputStream = getFsDataOutputStream(hdfs, currentFilePath);
            if (fsDataOutputStream == null) {
                closeAll(false);
                return false;
            }
        }
        return true;
    }

    private FSDataOutputStream getFsDataOutputStream(FileSystem hdfs, Path file) {
        FSDataOutputStream fsDataOutputStream = null;
        try {
            fsDataOutputStream = createFile(file);
        } catch (IOException e) {
            printStackTrace(e);
        }
        return fsDataOutputStream;
    }

    private FSDataOutputStream createFile(Path file) throws IOException {
        logger.debug(String.format("Creating file %s", file.toString()));
        hdfs.mkdirs(file.getParent());
        FSDataOutputStream fsDataOutputStream = hdfs.create(file);
        return fsDataOutputStream;
    }

    @SuppressWarnings("unused")
    private boolean isDfsSupportAppendEnabled(FileSystem hdfs) {
        // This is for future usage when we enable append
        return Boolean.valueOf(hdfs.getConf().get("dfs.support.append"));
    }

    @Override
    public void onMessageQueueEmpty() {
        if (fsDataOutputStream != null) {
            try {
                // Trying to flush however the effect is
                // questionable... (It is not guaranteed that data has been
                // flushed to persistent store on the datanode)
                logger.debug("Flushing hdfs");
                fsDataOutputStream.hflush();
                fsDataOutputStream.hsync();
            } catch (IOException e) {
                logger.debug(String.format("Flush failed, reason: %s", e.getMessage()));
            }
        }
    }

    @Override
    public void close() {
        logger.debug("Closing hdfs");
        closeAll(options.getArchiveDir() != null);
        isOpened = false;
    }

    private void closeAll(boolean isArchiving) {
        closeDataOutputStream();

        if (isArchiving) {
            archiveFile();
        }

        closeHdfs();
        isOpened = false;
    }

    private void closeDataOutputStream() {
        if (fsDataOutputStream != null) {
            try {
                fsDataOutputStream.close();
            } catch (IOException e) {
                fsDataOutputStream = null;
                printStackTrace(e);
            }
        }
    }

    private void closeHdfs() {
        if (hdfs != null) {
            try {
                hdfs.close();
            } catch (IOException e) {
                hdfs = null;
                printStackTrace(e);
            }
        }
    }

    private void archiveFile() {
        if (options.getArchiveDir() != null && currentFilePath != null) {
            logger.debug(String.format("Trying to archive %s to %s", currentFilePath.getName(), options.getArchiveDir()));
            Path archiveDirPath = new Path(String.format("%s/%s", options.getUri(), options.getArchiveDir()));
            try {
                hdfs.mkdirs(archiveDirPath);
                hdfs.rename(currentFilePath, archiveDirPath);
            } catch (IOException e) {
                logger.debug(String.format("Unable to archive, reason: %s", e.getMessage()));
            }
        }

    }

    private void printStackTrace(Throwable e) {
        String trace = org.apache.commons.lang.exception.ExceptionUtils.getStackTrace(e);
        logger.error(trace);
    }

    @Override
    public String getNameByUniqOptions() {
        return String.format("hdfs,%s,%s", options.getUri(), options.getFile());
    }

    @Override
    public void deinit() {
        logger.debug("Deinitialize hdfs destination");
        options.deinit();
    }
}
