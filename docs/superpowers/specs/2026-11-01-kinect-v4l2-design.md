# Kinect v2 Linux camera bridge

Target: Arch Linux. The user selected extending the existing C++ bridge and
delegated remaining choices and execution. Reuse libfreenect2 for the Xbox One
Kinect USB protocol and v4l2loopback for standard V4L2 camera nodes.

Default outputs at 30 fps:

| Node | Content | Resolution | Format |
| --- | --- | --- | --- |
| /dev/video10 | Color webcam | 1920x1080 | YUYV, BT.601 limited range |
| /dev/video11 | Infrared | 512x424 | GREY (optional YUYV for OpenCV compatibility) |
| /dev/video12 | Metric depth | 512x424 | Y16, little endian millimetres, zero invalid |

Y16 carries depth because upstream v4l2loopback supports it but does not list
Z16. Applications must interpret these values as millimetres, not luminance.
Provide optional 8-bit depth preview for ordinary viewers. No registration is
performed; color and depth retain their native optical coordinates.

Allow stream selection, serial selection, explicit CPU/OpenGL/OpenCL/CUDA
pipeline selection (reject unavailable pipelines), IR gain/gamma, and finite
frame counts. Default to CPU for a headless system service. Provide a synthetic
test pattern through the same conversion/output path, never enabled implicitly.

Reject invalid arguments before accessing hardware. Validate incoming frame
size, type and status. Validate V4L2 capabilities, negotiated format, dimensions,
stride and buffer size. Handle EINTR and bounded nonblocking output; treat short
writes and failed enabled outputs as errors. Set loopback timeout to blank stale
frames; stop with failure after a bounded period without sensor frames so systemd
can retry after unplug/replug. Release frames and stop/close USB on every exit.

Provide idempotent loopback setup that adds only missing Kinect devices, refuses
conflicting nodes and never unloads the shared module. Provide udev aliases
/dev/kinect-{rgb,ir,depth}, video-group permissions, systemd integration, staged
CMake installation, and Arch/Howdy instructions. Do not edit PAM or enrol faces.
The service continuously captures while running; on-demand power management and
depth-based authentication/liveness are outside this bridge's scope.

Alternatives: OpenNI2 exposes Kinect data to specialized applications but does
not directly meet webcam/Howdy V4L2 compatibility. A new kernel USB driver would
duplicate existing protocol/depth reconstruction and substantially expand scope.

Verification: deterministic pixel fixtures, argument tests without hardware,
V4L2 boundary tests with substituted system calls, shell/service validation and
a clean build. USB/video devices are unavailable in the Codex sandbox; real
Kinect streaming and Howdy face recognition require a subsequent hardware test.
