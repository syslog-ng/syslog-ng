`logmatcher`: fix named capture groups numbered 128 and above being captured incorrectly

The PCRE named-group number was decoded through a signed character, so any named capture
group whose number had its low byte >= 0x80 (128, 255, ...) sign-extended into a negative
ovector index, producing an incorrect or garbage captured value. The decode is now unsigned.
