# kinectcam

Use an **Xbox One Kinect (Kinect v2)** as a Linux webcam, infrared camera and
depth source. This C++ userspace bridge builds on
[libfreenect2](https://github.com/OpenKinect/libfreenect2) and
[v4l2loopback](https://github.com/v4l2loopback/v4l2loopback).
It does not support the Xbox 360 Kinect or Azure Kinect.

| Camera | Default device | Stable alias after udev setup | Frame format |
| --- | --- | --- | --- |
| Color | `/dev/video10` | `/dev/kinect-rgb` | 1920×1080 YUYV |
| Infrared | `/dev/video11` | `/dev/kinect-ir` | 512×424 GREY; optional YUYV |
| Depth | `/dev/video12` | `/dev/kinect-depth` | 512×424 Y16, millimetres |

All outputs are configured for 30 fps. Actual throughput depends on the USB
controller and processing pipeline. Separate applications can read the RGB,
IR and depth nodes simultaneously. On the tested v4l2loopback 0.15.4 module,
OBS and OpenCV compete for capture ownership of the same node: deactivate an
OBS source before opening that node in another program. Only one process should
capture the physical Kinect. Quit Protonect and other Kinect applications before
starting this bridge.

## Hardware and Arch dependencies

Use a powered Kinect v2 PC adapter and a USB **3.0** port/controller. USB 2 is
unsupported. See upstream [hardware requirements and troubleshooting](https://github.com/OpenKinect/libfreenect2#requirements).

```sh
sudo pacman -Syu --needed base-devel cmake python libusb turbojpeg \
    v4l2loopback-dkms v4l2loopback-utils v4l-utils
```

Install headers matching **your running kernel**. The stock Arch `linux` kernel
uses `linux-headers`; `linux-lts` uses `linux-lts-headers`. A custom kernel needs
its own matching headers/build tree, rather than the stock `linux-headers`.
Check `uname -r`, `dkms status` and `/usr/lib/modules/$(uname -r)/build`.
[Arch package details](https://archlinux.org/packages/extra/any/v4l2loopback-dkms/)
and [utility package](https://archlinux.org/packages/extra/any/v4l2loopback-utils/).

Install `libfreenect2` (available through the AUR, or build it from upstream).
To build a minimal CPU-capable installation directly:

```sh
git clone https://github.com/OpenKinect/libfreenect2.git
cmake -S libfreenect2 -B libfreenect2/build \
    -DCMAKE_INSTALL_PREFIX=/usr/local -DBUILD_EXAMPLES=OFF \
    -DENABLE_OPENGL=OFF -DENABLE_OPENCL=OFF -DENABLE_CUDA=OFF
cmake --build libfreenect2/build -j2
sudo cmake --install libfreenect2/build
sudo ldconfig
```

The bridge uses CPU depth processing by default so it can run without a display
server. For higher performance, build libfreenect2 with OpenCL or another supported
pipeline and choose `--pipeline opencl`, `gl`, `cuda` or `auto` explicitly.
Unavailable pipelines are rejected instead of silently falling back.

## Build and run

From this project directory:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

If libfreenect2 was installed to an unusual prefix, pass
`-DCMAKE_PREFIX_PATH=/path/to/prefix`. Python is only needed for tests; use
`-DBUILD_TESTING=OFF` for an installation without the test suite.

Install the executable, setup helper, udev rules, service and configuration example:

```sh
sudo cmake --install build
sudo udevadm control --reload-rules
sudo usermod -aG video "$USER"
```

Log out and back in for new group membership. Unplug/reconnect the Kinect adapter
after reloading rules. If loopbacks already existed, refresh their rules with
`sudo udevadm trigger --subsystem-match=video4linux` and `sudo udevadm settle`.
The USB rules grant the `video` group access to the Kinect, and label-based video
rules create the stable aliases. Installation does not enable the service or
change authentication settings.

```sh
sudo bash setup-loopback.sh
./build/kinectcam --list
./build/kinectcam --pipeline cpu --ir-format yuyv
```

Start the producer **before** opening a webcam application. The loopbacks use
`exclusive_caps=1`, so capture applications discover them once the producer is
attached. Select **KinectRGB** in a webcam application. Test locally with:

```sh
ffplay -f v4l2 -input_format yuyv422 -video_size 1920x1080 /dev/kinect-rgb
ffplay -f v4l2 -input_format yuyv422 -video_size 512x424 /dev/kinect-ir
```

These examples require `ffmpeg`. If running the default GREY IR output, use
`-input_format gray` for its preview instead.

The setup helper adds only missing devices and never unloads `v4l2loopback`, so
it preserves other virtual cameras. Preview its commands with `--dry-run`.
If video10–12 are occupied, choose free numbers and matching bridge paths:

```sh
sudo bash setup-loopback.sh --video-nr 20,21,22
./build/kinectcam --rgb-dev /dev/video20 --ir-dev /dev/video21 --depth-dev /dev/video22
```

The installed systemd service's helper uses 10–12. To use custom numbers in that
service, override `ExecStartPre` and set matching paths in `KINECTCAM_ARGS`.

## Howdy

The bridge supplies IR imagery; Howdy handles face recognition. The service uses
`--ir-format yuyv` for broad OpenCV compatibility, with no color information in
that IR stream. The CLI also offers `--ir-format grey` for native 8-bit consumers.
This matches [Howdy's V4L2 capture interface](https://github.com/boltgolt/howdy/blob/master/howdy/src/recorders/video_capture.py).

Keep OBS and other capture clients off `/dev/kinect-ir` while using Howdy.
They may use `/dev/kinect-rgb` at the same time.

Start the service, then edit Howdy's settings:

```sh
sudo systemctl daemon-reload
sudo systemctl enable --now kinectcam.service
sudo howdy config
```

In the existing `[video]` section, set:

```ini
device_path = /dev/kinect-ir
recording_plugin = opencv
frame_width = 512
frame_height = 424
device_fps = 30
force_mjpeg = false
```

Then use `sudo howdy test` to check the live view and `sudo howdy add` when you
are ready to enrol. Do not point Howdy at the color or depth device. This project
does not change PAM settings or implement depth-based liveness/spoof detection.

If faces appear dark, copy the configuration example and adjust gain/gamma:

```sh
sudo install -m 0644 /usr/share/kinectcam/kinectcam.conf.example /etc/kinectcam.conf
sudoedit /etc/kinectcam.conf
sudo systemctl restart kinectcam
```

`--ir-gain 4 --ir-gamma 1` is the default. Gain multiplies intensity; gamma above
1 brightens shadows. Avoid washing out facial detail. The service continuously
captures while running; add `--no-rgb --no-depth` to its arguments for IR-only
use. IR capture still uses the Kinect's shared IR/depth hardware pipeline.

Do not use `--test-pattern` on a device configured for authentication. Access to
write the loopback is access to supply its images; the `video` group is trusted
by this configuration, as with other userspace virtual cameras.

## Depth data

Default depth is **metric data**, not an 8-bit picture. Each pixel is a
little-endian unsigned 16-bit integer in **millimetres**, rounded from the
libfreenect2 float value. Zero indicates invalid/missing data; out-of-range
values saturate at 65535. `v4l2loopback` supports `Y16` but does not list `Z16`
in its [format table](https://github.com/v4l2loopback/v4l2loopback/blob/main/v4l2loopback_formats.h),
so applications must apply these depth semantics explicitly.

For example, capture 30 raw depth frames:

```sh
v4l2-ctl --device=/dev/kinect-depth --stream-mmap --stream-count=30 --stream-to=depth.y16
```

Each tightly packed 512×424 frame occupies 434176 bytes; inspect the negotiated
stride with `v4l2-ctl --get-fmt-video` if using a different loopback version.
In Python/NumPy, read a tightly packed frame using `dtype='<u2'` and reshape it
to `(424, 512)`. These are native depth-camera coordinates; the bridge does not
register depth to the 1920×1080 color camera.

For a viewable preview, restart the bridge with
`--depth-format grey --depth-max-mm 4500`, then use
`ffplay -f v4l2 -input_format gray -video_size 512x424 /dev/kinect-depth`.
Preview mode discards metric precision.

## Diagnostics and verification

```sh
./build/kinectcam --help
systemctl status kinectcam
journalctl -u kinectcam -b
v4l2-ctl --list-devices
```

For a no-sensor output test, stop the normal service and run
`./build/kinectcam --test-pattern --frames 150`. This still needs real loopback
devices and writes synthetic images to them. The CTest integration test instead
uses a separate test executable with substituted kernel syscalls and inspects
actual converted frame bytes; fake-device handling is absent from the installed
binary.

The writer checks capabilities, format negotiation, stride and buffer sizes;
short writes or failed enabled outputs stop the process with an error. Input
silence for five seconds causes failure so systemd can retry after reconnection.
Loopback timeout is set to 1000 ms so a stalled producer yields its timeout
image (blank on fresh devices), rather than indefinitely repeating a face.

If the output is reported busy, stop another producer. If a previous tool set
`keep_format=1`, stop consumers and clear it using
`v4l2-ctl -d /dev/video10 -c keep_format=0` (repeat for affected devices).
For USB transfer errors, consult upstream troubleshooting; USB controller support
and USBFS buffer limits can affect capture.

**Validation boundary:** automated pixel, argument, V4L2-boundary, setup and
synthetic-stream tests can run without a Kinect. Actual Kinect capture, kernel
loopback delivery, desktop webcam discovery and Howdy recognition must also be
tested on the target hardware; they cannot be verified in a sandbox without
USB and video-device access.

For the actual build checks and connected sensor's startup-timeout result from
this development session, see [docs/validation.md](docs/validation.md).

## AUR packaging

The `kinectcam-git` recipe is in `packaging/aur`. The manual Gitea workflow builds
and tests it on x86_64 before publishing metadata from ARM64. It reads both
`AUR_SSH_KEY` and `AUR_REPO_URL` from repository secrets. See
[the publishing guide](docs/aur-publishing.md) for setup and first publication.

## License

kinectcam is licensed under **GPL-3.0-only**. See [LICENSE](LICENSE).
libfreenect2 and v4l2loopback retain their own licenses.
