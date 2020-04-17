`example-msg-generator`: support `freq(0)` for fast message generation

log {
   source { example-msg-generator(freq(0) num(100)); };
   destination { file("/dev/stdout"); };
};
