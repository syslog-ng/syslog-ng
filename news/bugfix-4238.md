`riemann`: fixed severity levels of Riemann diagnostics messages, the error
returned by riemann_communicate() was previously only logged at the trace
level and was even incomplete: not covering the case where
riemann_communicate() returns NULL.
