`geoip2`: Fixed the parser stopping after the first `names` map (for example only `city.names.en`), leaving out
country, continent and location, depending on uninitialized memory.
