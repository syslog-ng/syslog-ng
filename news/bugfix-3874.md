`disk-buffer()`: fix a disk-buffer corruption issue

A completely filled and then emptied disk-buffer may have been recognised as corrupt.
