/*
 * Copyright (c) 2015 BalaBit
 * All Rights Reserved.
 * Authors: Zoltan Pallagi <zoltan.pallagi@balabit.com>
 */

package org.syslog_ng.hdfs;

import org.syslog_ng.*;

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

    private String hdfsUri;
    private String hdfsFile;
    private String hdfsArchiveDir;
    private String hdfsResources;

    private FileSystem hdfs;
    private FSDataOutputStream fsDataOutputStream;

    private Path currentFilePath;

    private boolean isOpened;
    private int maxFileNameLength = 255;

    public HdfsDestination(long handle) {
        super(handle);
    }

    private void printErrorMessage(String msg) {
        InternalMessageSender.error(String.format(Locale.US, "%s ERROR: %s", LOG_TAG, msg));
    }

    private void printDebugMessage(String msg) {
        InternalMessageSender.debug(String.format(Locale.US, "%s %s", LOG_TAG, msg));
    }

    private boolean getRequiredOptions() {
        hdfsUri = getOption("hdfs_uri");
        if (hdfsUri == null) {
            printErrorMessage("Missing hdfs_uri!");
            return false;
        }

        hdfsFile = getOption("hdfs_file");
        if (hdfsFile == null) {
            printErrorMessage("Missing hdfs file!");
            return false;
        }
        return true;
    }

    private void getNonRequiredOptions() {
        hdfsArchiveDir = getOption("hdfs_archive_dir");
        hdfsResources = getOption("hdfs_resources");
        String lengthOption = getOption("hdfs_max_filename_length");
        if (lengthOption != null) {
            maxFileNameLength = Integer.valueOf(lengthOption);
        }
    }

    @Override
    public boolean init() {
        printDebugMessage("Initialize hdfs destination");
        disableLog4jWarningMessages();

        if (!getRequiredOptions()) {
            printErrorMessage("Required option(s) are missing, exiting!");
            return false;
        }
        getNonRequiredOptions();

        return true;
    }

    private void disableLog4jWarningMessages() {
        // Disable annoying warning messages of the hdfs external dependency
        Logger.getRootLogger().setLevel(Level.OFF);
    }

    @Override
    public boolean isOpened() {
        return isOpened;
    }

    @Override
    public boolean open() {
        printDebugMessage("Opening hdfs");
        isOpened = false;
        try {
            Configuration configuration = new Configuration();
            loadConfigResources(configuration);

            hdfs = FileSystem.get(new URI(hdfsUri), configuration);
            hdfs.getStatus(); // Just to be sure we are really connected to hdfs
            isOpened = true;
        } catch (Exception | Error e) {
            printStackTrace(e);
            closeAll(false);
        }
        return isOpened;
    }

    private void loadConfigResources(Configuration configuration) {
        if (hdfsResources != null) {
            String[] resources = hdfsResources.split(";");
            for (int i = 0; i < resources.length; i++) {
                String resource = resources[i];
                configuration.addResource(new Path(resource));
                printDebugMessage(String.format("Resource loaded: %s", resource));
            }
        }
    }

    @Override
    public boolean send(String message) {
        isOpened = false;

        if (!ensureDataOutputStream()) {
            return false;
        }

        try {
            printDebugMessage("Outgoing message: " + message);
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
            String currentFile = String.format("%s.%s", hdfsFile, UUID.randomUUID());
            currentFilePath = new Path(String.format("%s/%s", hdfsUri, currentFile));

            if (currentFilePath.getName().length() > maxFileNameLength) {
                String fileName = truncateFileName(currentFilePath.getName(), maxFileNameLength);
                printDebugMessage(String.format("Maximum file name length (%s) exceeded, truncated to %s", maxFileNameLength, fileName));

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
        printDebugMessage(String.format("Creating file %s", file.toString()));
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
                printDebugMessage("Flushing hdfs");
                fsDataOutputStream.hflush();
                fsDataOutputStream.hsync();
            } catch (IOException e) {
                printDebugMessage(String.format("Flush failed, reason: %s", e.getMessage()));
            }
        }
    }

    @Override
    public void close() {
        printDebugMessage("Closing hdfs");
        closeAll(hdfsArchiveDir != null);
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
        if (hdfsArchiveDir != null && currentFilePath != null) {
            printDebugMessage(String.format("Trying to archive %s to %s", currentFilePath.getName(), hdfsArchiveDir));
            Path archiveDirPath = new Path(String.format("%s/%s", hdfsUri, hdfsArchiveDir));
            try {
                hdfs.mkdirs(archiveDirPath);
                hdfs.rename(currentFilePath, archiveDirPath);
            } catch (IOException e) {
                printDebugMessage(String.format("Unable to archive, reason: %s", e.getMessage()));
            }
        }

    }

    private void printStackTrace(Throwable e) {
        String trace = org.apache.commons.lang.exception.ExceptionUtils.getStackTrace(e);
        printErrorMessage(trace);
    }

    @Override
    public void deinit() {
        printDebugMessage("Deinitialize hdfs destination");
    }
}
