`wildcard-file()`: fix a crash and detection of file delete/move when using ivykis poll events

Two issues were fixed

- Fixed a crash in log pipe queue during file deletion and EOF detection (#4989)

   The crash was caused by a concurrency issue in the EOF and file deletion detection when using a `wildcard-file()` source.

   If a file is written after being deleted (e.g. with an application keeping the file open), or if these events happen concurrently, the file state change poller mechanism might schedule another read cycle even though the file has already been marked as fully read and deleted.

   To prevent this re-scheduling between these two checks, the following changes have been made:
   Instead of maintaining an internal EOF state in the `WildcardFileReader`, when a file deletion notification is received, the poller will be signaled to stop after reaching the next EOF. Only after both conditions are set the reader instance will be deleted.

- Fixed the file deletion and removal detection when the `file-reader` uses `poll_fd_events` to follow file changes, which were mishandled. For example, files that were moved or deleted (such as those rolled by a log-rotator) were read to the end but never read again if they were not touched anymore, therefore switching to the new file never happened.
