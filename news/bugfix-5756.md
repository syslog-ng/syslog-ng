`java`: Due to a known JVM dlopen/dlclose limitation, `java` module has been disabled in the
official binaries. The module is still available to compile.

`grpc`: Fixed dynamic linking to prevent shared objects from leaking RSS on reload.
