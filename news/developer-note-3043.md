`signal-slot-connector`: a generic event handler interface for syslog-ng modules

* The concept is simple:

There is a SignalSlotConnector which stores Signal - Slot connections 
Signal : Slot = 1 : N, so multiple slots can be assigned to the same Signal.
When a Signal is emitted, the connected Slots are executed.
Signals are string literals and each module can define its own signals.

* Interface/protocol between signals and slots

A macro (SIGNAL) can be used for defining signals as string literals:

```
SIGNAL(module_name, signal, signal_parameter_type)
```

The parameter type is for expressing a kind of contract between signals and slots.

usage:

```
   #define signal_cfg_reloaded SIGNAL(config, reloaded, GlobalConfig)

   the generated literal:

   "config::signal_reloaded(GlobalConfig *)"
```
 
 emit passes the argument to the slots connected to the emitted signal.

