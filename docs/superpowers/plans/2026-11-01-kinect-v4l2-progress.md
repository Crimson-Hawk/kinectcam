# Execution ledger — plan: 2026-11-01-kinect-v4l2.md

Work in place: supplied .git is empty/read-only, so Git worktree/commit scripts
cannot run. User delegated implementation choices and asked to proceed.

Task 1 complete: conversion fixtures failed without the new interface; CLI
tests reproduced seven baseline failures. New conversion/options implementation
passes pixel fixtures and all CLI checks.

Task 2 core complete: V4L2 tests failed without the writer. Writer now passes
padding, format substitution, undersized buffers, short writes, EINTR/EAGAIN,
descriptor cleanup and stale-frame timeout checks. Real capture has RAII guards,
frame validation, five-second input timeout and synthetic mode.

Source investigation: installed v4l2loopback treats timeout_image_io as a trigger,
even for value zero. Added a regression fixture, observed failure, removed the
call, observed green. keep_format would persist after SIGKILL and hide OUTPUT
from a restarted exclusive-capability writer. Added failing fixture and removed
that unnecessary persistent lock; normal ownership already constrains consumers.

Current verification: build succeeds; CTest 3/3 pass. USB and /dev/video nodes
are unavailable to this sandbox; live kernel/Howdy verification remains manual.

Task 2 integration complete: full executable synthetic tests verify all three
payload sizes, metric depth bytes, and IR-only YUYV routing. Fake kernel syscalls
are linked only into a test binary, never the installed executable.

Task 3 complete: idempotent setup and conflict refusal tests pass; clean Release
build passes CTest 5/5. BUILD_TESTING=OFF builds successfully. Staged installation
has correct executable/file modes. udevadm verify succeeds. systemd-analyze
verify succeeds outside the sandbox against a temporary copy whose executable
paths reference the staged installation. Source unit points to /usr correctly.

Final independent read-only review: no Critical/Important actionable findings;
reviewer checked actual installed v4l2loopback 0.15.4 implementation and upstream
utility flags/fields, then reran all five CTest suites successfully.

Live test attempt outside sandbox: actual Kinect detected/opened over USB 3,
but startup failed with LIBUSB_ERROR_TIMEOUT. Original baseline executable and
upstream Protonect cpu -noviewer -norgb -frames 10 reproduce the same failure.
No live camera images were saved; test adapter sinks pointed at /dev/null.
Sensor is on a two-hub path; speed 5000, power/control=on, runtime_status=active.
v4l2loopback module is installed for the custom running kernel but unloaded;
v4l2loopback-ctl is missing; sudo -n requires a password. No package installation,
module loading, udev changes, service enablement or PAM modification performed.
See docs/validation.md for the remaining hardware/setup actions.

Follow-up after user changed USB port: Kinect now enumerates on bus 002 at
5000 Mb/s and starts successfully. Ran real bridge capture/conversion with CPU,
YUYV IR and Y16 depth for 60 synchronized frame sets, using the test-only kernel
adapter and /dev/null sinks. Exited 0 with clean stop/close. Some depth-packet-loss
warnings remain; sustained 30 fps is not established. Missing loopback utility,
unloaded module and sudo password still block real V4L2/Howdy verification.

Follow-up after user installation: real systemd service active, zero restarts,
three udev aliases and expected capture formats present. Simultaneous OpenCV5
reads of 25 frames per stream passed; all24 transitions changed; raw uint16 depth
valid fraction .889, sampled range622..3730mm. IR decoded grayscale with identical
BGR channels and Howdy-style cvtColor passed. Throughput10.7 sets/sec in CPUmode.
OpenCV EBUSY traced to user's OBS source; user released it and all3 nodes passed.
README now documents same-node consumer contention. Howdy2.6.1 is installed but
its root-only directory prevents actual reader testing without sudo. Authentication
settings unchanged. Service running, boot enablement not requested/performed.

Final suite rerun found setup fixtures were coupled to real sysfs video10..12:
after live installation, fake query reported absent while real sysfs existed,
correctly triggering collision refusal. Tests now choose three unused video IDs
and pass them explicitly to the fake-tool setup; no production behavior changed.
