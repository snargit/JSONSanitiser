# JSONSanitiser
This is a port of the [OWASP json-sanitizer](https://github.com/OWASP/json-sanitizer) Java version to C++. It expects the JSON to be UTF-8 text. At present it does not validate that the input is correct UTF-8.

The library itself has no external dependencies, but does require C++17. The tests use the [google test framework](https://github.com/google/googletest). Additionally, the fuzzing test requires a recent version of [boost](https://www.boost.org/).
