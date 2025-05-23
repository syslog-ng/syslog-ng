`wildcard-file`: Added inotify-based regular file change detection using the existing inotify-based directory monitor.

This improves efficiency on OSes like Linux, where only polling was available before, significantly reducing CPU usage while enhancing change detection accuracy.

To enable this feature, inotify kernel support is required, along with `monitor-method()` set to `inotify` or `auto`, and `follow-freq()` set to 0.
