# cpp-httplib

Vendored single-header HTTP library for the C++ backend.

- Upstream: https://github.com/yhirose/cpp-httplib
- Version: v0.48.0
- Release: https://github.com/yhirose/cpp-httplib/releases/tag/v0.48.0
- Header source: https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.48.0/httplib.h
- Local file: `third_party/cpp-httplib/httplib.h`

Update process:

1. Pick a tagged upstream release.
2. Download `httplib.h` from the matching tag, not from a moving branch.
3. Replace the local header.
4. Update this README with the new version and source URL.
5. Build `shuati_server` and run `shuati_tests`.
