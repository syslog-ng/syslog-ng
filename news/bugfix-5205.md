slog: New implementation of the pseudo-random function

The previous implementation allowed an attacker to distinguish between
the pseudo-random function (PRF) and a real random function by supplying
specially crafted inputs to it. This leads to a predictable way of how the
PRF is generating output which should not by allowed by a good PRF.
The new implementation provides a variable input length and
constant output length PRF based on AES CMAC for key derivation using
the current key K<sub>i</sub>, i.e. a key expansion of K<sub>i</sub> using 
multiple iterations is performed. Subsequently, with K<sub>i</sub> as the 
key used in the PRF, the input is defined as
```
(i || label || 0x00 || context || L)
```
with `i` as the number of the current iteration, `L` as the total length of
the requested output, `label` and `context` for parametrization of the PRF
output based on the intended purpose. See NIST reports SP-800-56cr2 and
SP.800-108r1 for more information on key derivation schemes with a PRF.

In the previous implementation, the input length was fixed
at 16 bytes regardless of the actual length of the input data.
This is also fixed in the new implementation.
