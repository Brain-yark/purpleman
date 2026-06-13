PurpleMan — Help & Usage
=========================

Overview
--------
PurpleMan is a modular hybrid controller/implant project with network and offline (USB) channels. This document describes how to build, test, run, and safely experiment with the code in a home lab.

What is this software?
This project is provided as a research and teaching prototype that demonstrates hybrid command-and-control (C2) communication techniques for defensive research, red-team exercises, and controlled lab experiments. It is not intended for unauthorized monitoring, intrusion, or malicious activity.

Quick safety reminder
- Only run this software on systems you own or have explicit written permission to test.
- Use isolated VMs, snapshots, and network segmentation to prevent accidental impact on other systems.
- Enabling `-DTESTING=ON` reduces persistence and network side effects but does not eliminate all risk; always validate in an isolated lab.

Quick build & test
------------------
1. Configure + build (Release):

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

2. Run unit tests:

```powershell
cd build
ctest -C Release --output-on-failure
```

C2 Controller (`c2_controller`)
--------------------------------
This project includes a controller executable (`c2_controller`) built from `c2_contoller.cpp`. Use the controller to manage implants in your lab.

Build: the controller is produced by the normal build step above (`c2_controller.exe` in `build/Release`).

Run (example):

```powershell
# Run controller with default settings
.\build\Release\c2_controller.exe

# Run with verbose logging (example flag - adjust implementation as needed)
.\build\Release\c2_controller.exe --verbose

Runtime command examples
------------------------
- Start controller and use the built-in prompt:

```powershell
.\build\Release\c2_controller.exe
```

- At the `c2_controller` prompt, view runtime command help:

```text
help
```

- Example controller commands:

```text
list
info <implant-id>
interact <implant-id>
exec <implant-id> sysinfo
broadcast screenshot
usb_status
usb_pack <implant-id> "exec whoami"
usb_results
stats
save
exit
```
```

CLI / Config notes:
- The controller reads runtime configuration from the `ConfigManager` (see `include/config/manager.h`).
- If you add CLI flags, document them here. Common flags to add: `--bind-address`, `--port`, `--log-file`, `--testing`.
- For safe experiments, run the controller and implants on isolated VMs and use `-DTESTING=ON` on implant builds.

Integration tips
- To test end-to-end in the lab, run `c2_controller` on one VM and the implant (`pown` build) on another isolated VM. Use `MockNetworkClient` or `-DTESTING=ON` to avoid external network calls during development.

Phishing payload generator
--------------------------
- Build or place `phishing_gen.exe` alongside the implant binary.
- Generate phishing delivery files for the implant.

Example payload generation commands:

```powershell
.\build\Release\phishing_gen.exe .\build\Release\pown.exe all
.\build\Release\phishing_gen.exe .\build\Release\pown.exe doc
.\build\Release\phishing_gen.exe .\build\Release\pown.exe html
.\build\Release\phishing_gen.exe .\build\Release\pown.exe vbs
.\build\Release\phishing_gen.exe .\build\Release\pown.exe iso
.\build\Release\phishing_gen.exe .\build\Release\pown.exe lnk
```

If `phishing_gen.exe` is not built yet, add it as a target in `CMakeLists.txt` or build it manually from `phising/phising_payloadgen.cpp`.


TESTING mode
------------
A compile-time `TESTING` option disables persistence, real network calls, and certain background threads so you can run the code safely in a lab.

To enable:

```powershell
cmake -S . -B build -DTESTING=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Behavior under `TESTING`:
- `InstallPersistence()` is not executed.
- `TryTCPConnection`, `TryHTTPSConnection`, and `TryDNSTunneling` return without making network calls.
- Heartbeat/network threads are not started.

Safe integration testing recommendations
---------------------------------------
- Use isolated virtual machines or containers and snapshots.
- Place test VMs on an isolated VLAN with no internet access (or firewall rules limiting egress).
- Use `-DTESTING=ON` for early tests and unit-driven validation.
- Replace `NetworkClient` with a `MockNetworkClient` in tests to simulate C2 responses.
- Stub out persistence (registry, file attributes) during tests, or run in accounts with no elevated privileges.

Suggested next steps to create a test harness
--------------------------------------------
1. Add `MockNetworkClient` (in `include/network/mock_client.h` and `src/network/mock_client.cpp`) implementing the `NetworkClient` interface; add unit tests invoking `HybridImplant` methods.
2. Add an integration test binary that composes `HybridImplant` with `MockNetworkClient` and verifies command/result flow.
3. Add `-DTESTING=ON` to CI for integration tests that should not modify hosts.

How to document new features
----------------------------
- Add short usage notes to `docs/HELP.md` with command examples.
- Add a small `--help` output in `c2_controller` or `pown` showing available runtime flags (planned work).

Safety & Legal
--------------
Only run this code on systems you own or explicitly control and for which you have written authorization. The project contains code that interacts with system persistence and networking which can be disruptive. Exercise caution and restore snapshots after experiments.

Contact / Contributions
-----------------------
- For help extending tests or adding mock clients, ask in the project issue tracker or open a PR with a small change and tests.

Files/paths added or touched
---------------------------
- docs/HELP.md  — this file (comprehensive help and instructions)

If you want, I can now:
- Add a `MockNetworkClient` and a basic integration test harness, or
- Add a `--help` CLI output to `c2_controller`/`pown.cpp`, or
- Generate a README.md from this help page.

Which would you like next?