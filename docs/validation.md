# Verification on 2026-11-01

## On-demand capture

The service now defaults to `--on-demand`; the standalone CLI remains continuous
unless that flag is supplied. All **eight CTest suites** pass, including demand
grace timing, event draining, actual queue-size blanking and synthetic
idle/wake/idle/wake transitions. Independent review found a queue-size mismatch
between S_PARM and allocated buffers; QUERYBUF now determines the actual count,
with a regression test observed failing before the fix. A live test exposed
ENOENT for empty nonblocking event queues; fixtures reproduce it and the bridge
now handles both ENOENT and EAGAIN.

After the user stopped the old service, the new bridge ran temporarily against
the real Kinect and kernel loopback devices. With CPU processing, YUYV IR and all
three outputs enabled, cold idle did not open a physical capture session. Two
OpenCV IR capture sessions each read five nonblank frames, with first usable
frames after **1.295s** and **1.080s**. After each client closed, the bridge stopped
streams, released USB interfaces and closed the device; no spontaneous wake
occurred during the subsequent two-second idle checks. It then exited cleanly
with status 0. No images were saved. Depth-packet-loss warnings remain, as in
previous continuous tests. This verifies capture shutdown and USB release, not
electrical power removal or a measured reduction in wall power.

A clean Arch x86_64 package build of the final source passed all eight suites
and namcap reported no package errors; the full local suite also passed.
The user subsequently installed the updated files and reloaded/restarted the
service. Its actual arguments include `--on-demand`; startup remained idle.
A further OpenCV consumer read five nonblank IR frames, first usable frame at
**1.044s**. The systemd service then stopped and closed the sensor, remaining
active with zero restarts. Howdy's actual authentication remains untested.

## GitHub mirror update

AUR's homepage/source and the CI local-source redirect now use
https://github.com/Crimson-Hawk/kinectcam. The master sync workflow and the manual
AUR workflow use Gitea secret `GH_MIRROR_TOKEN`. The latter requires a successful
sync of its validated commit before AUR publication.

Seven mirror unit tests exercise real local Git repositories: initial/update,
no-op, unrelated refs, divergent history, missing credentials, private feature
commits and temporary askpass credential handling. Annotated tags are explicitly
excluded even with inherited push.followTags enabled; the regression was observed
failing before adding --no-follow-tags. The full local suite passes seven CTest
suites. A clean Arch package build and the publishing container's metadata/scanner
preparation both passed with the GitHub source URL. Actual GitHub authentication
and Gitea-runner synchronization require the configured PAT and a live CI run.

## AUR packaging verification

The new `kinectcam-git` recipe was built in a disposable official Arch base-devel
x86_64 container, with its libfreenect2 AUR dependency built from a pinned recipe.
The package was created successfully and all **six CTest suites** passed,
including six real-Git publication tests. No camera devices were mounted into
the container and no host service was changed. Namcap found no package errors;
warnings cover explicitly required shell/runtime tools and implicitly satisfied
C++ library dependencies.

The publishing job's non-secret preparation steps ran successfully in the actual
`node:24-bookworm` container image on local x86_64. Debian makepkg generated
matching `.SRCINFO`, and pinned/checksummed Gitleaks 8.30.1 scanned history, source
and outgoing metadata without findings. The ARM64 runner and live Gitea/AUR
authentication/publication have not been exercised. Independent source review
found no Critical/Important issues. See [the publishing guide](aur-publishing.md).

## Passed

- Clean CMake Release build and CTest: **5/5 suites** (pixel fixtures, V4L2
  boundary errors/negotiation/padding, CLI validation, synthetic application
  streams, setup idempotency/conflicts).
- Production build with `BUILD_TESTING=OFF`.
- Staged install: executable, helper, udev rules, systemd unit and config example.
- `bash -n setup-loopback.sh`; dry-run performs no privileged actions.
- `udevadm verify packaging/99-kinectcam.rules`.
- `systemd-analyze verify` with the staged executable paths substituted into a
  temporary copy (actual `/usr` installation intentionally not performed).
- Independent source review: no Critical/Important findings. V4L2 behavior
  checked against the installed v4l2loopback 0.15.4 kernel source.

## Live test result after changing USB ports

Moving the Kinect to bus 002 resolved stream startup. The bridge successfully
captured, validated and converted **60 synchronized RGB/IR/depth frame sets**
with the CPU pipeline, YUYV IR compatibility mode and metric Y16 depth. It then
stopped and closed the device cleanly (exit status 0). The test-only output
adapter sent pixels to `/dev/null`; no images were saved. Actual kernel V4L2
delivery was subsequently verified as described below.

libfreenect2 logged several lost-depth-packet warnings during this run. This
proves capture/conversion works on the new port, but does not establish sustained
30 fps or loss-free delivery.

## Real V4L2 verification after installation

The user installed the helper utility, executable, rules and service, then
started `kinectcam.service`. The service runs successfully with zero restarts;
all three aliases are present and advertise capture formats:

| Alias | Format | Resolution |
| --- | --- | --- |
| /dev/kinect-rgb | YUYV | 1920×1080 |
| /dev/kinect-ir | YUYV | 512×424 |
| /dev/kinect-depth | Y16 | 512×424 |

OpenCV 5.0.0 simultaneously read **25 frames from each real kernel device**.
All streams changed on all 24 frame transitions. Color and IR decoded into
three-channel uint8 images; IR channels were identical, and
`cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)` succeeded, matching Howdy's processing.
IR mean intensity was 73.6/255 in the sampled frame.

Raw depth decoded into a 424×512 little-endian uint16 map with 88.9% nonzero
pixels and a sampled valid range of 622–3730 mm. The full test took 2.34 seconds,
approximately **10.7 observed frame sets per second**; the nominal advertised
30 fps is not the achieved CPU-pipeline rate on this machine. No images saved.

An initial OpenCV EBUSY was traced to OBS holding the same node. After the user
released OBS, the simultaneous three-node test succeeded. Use one negotiated
capture client per node with the tested module; RGB and IR can still be used
by different applications at the same time.

Howdy 2.6.1 is installed, but its root-only directory prevents inspection or
execution of its actual reader without sudo. OpenCV compatibility is verified;
Howdy's own test and face recognition remain unverified. No authentication
configuration or face models were changed. The service is running but is not
enabled for automatic boot startup.

## Initial live test result on the previous port

Outside the sandbox, the connected Microsoft Xbox NUI Sensor (`045e:02c4`)
enumerates and opens with libfreenect2, but stream startup fails with:

```text
bulk transfer failed: LIBUSB_ERROR_TIMEOUT Operation timed out
failed to start Kinect streams
```

The new capture code, the original baseline executable and upstream
`Protonect cpu -noviewer -norgb -frames 10` reproduce this startup failure.
This establishes that it also occurs without the new bridge. It does not
establish whether the cause is sensor power/state, the adapter/hub topology,
the installed libfreenect2, or the custom kernel/USB controller.

The sensor reports 5000 Mb/s at `/sys/bus/usb/devices/6-1.4.1`, behind two hubs.
Its `power/control` is `on` and `power/runtime_status` is `active`, so USB
autosuspend is already disabled for that device. No live frames were saved.

## Remaining setup and hardware checks

1. Keep using the new bus 002 port, where startup and 60 frame sets succeeded.
   Check sustained throughput and investigate the depth-packet-loss warnings if
   they persist. See [upstream troubleshooting](https://github.com/OpenKinect/libfreenect2/wiki/Troubleshooting).
2. Configure Howdy's device path and test its live view using the
   [README instructions](../README.md). Actual recognition/enrolment remains a
   user action; the bridge's IR camera is verified through OpenCV.
3. If automatic boot startup is wanted, run `sudo systemctl enable kinectcam`.
4. Unplug/replug recovery remains unverified; the service's restart handling is
   implemented but was not exercised against physical disconnection.
