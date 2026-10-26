# Westeros Sink L1

This suite builds the production raw-sink implementation with real GStreamer, GLib, and Wayland client libraries. A controllable SoC backend double and protocol/resource-manager doubles isolate hardware and compositor services; GStreamer objects and event/query APIs are not stubbed.

Configure with `cmake -S . -B build -DENABLE_COVERAGE=ON`, build with `cmake --build build --parallel`, then run `ctest --test-dir build -L WesterosSink --output-on-failure`. Generate the measured report with `cmake --build build --target coverage-westeros-sink`.

Coverage is instrumented and filtered to the production translation unit `westeros-sink.c`; SoC backend implementations, protocol doubles, and test code are outside the reported source scope. The report target enforces at least 75% line coverage for this measured scope; verify that the current tests meet the threshold on Linux. Hardware-dependent SoC behavior and optional software decoding are intentionally not part of this first L1 boundary.