#!/usr/bin/env bash
# Create only the Kinect loopback devices; never unload a shared module.
set -euo pipefail

dry_run=false
numbers=10,11,12
while (($#)); do
    case "$1" in
        --dry-run) dry_run=true; shift ;;
        --video-nr)
            [[ $# -ge 2 ]] || { echo '--video-nr requires RGB,IR,DEPTH numbers' >&2; exit 1; }
            numbers=$2; shift 2 ;;
        -h|--help)
            echo "Usage: $0 [--dry-run] [--video-nr 10,11,12]"
            exit 0 ;;
        *) echo "Unknown argument: $1" >&2; exit 1 ;;
    esac
done
[[ $numbers =~ ^(0|[1-9][0-9]{0,2}),(0|[1-9][0-9]{0,2}),(0|[1-9][0-9]{0,2})$ ]] || {
    echo 'Expected three distinct video numbers in 0..255' >&2; exit 1;
}
IFS=, read -r -a ids <<< "$numbers"
[[ ${ids[0]} != "${ids[1]}" && ${ids[0]} != "${ids[2]}" && ${ids[1]} != "${ids[2]}" ]] || {
    echo 'Video numbers must be distinct' >&2; exit 1;
}
for id in "${ids[@]}"; do
    ((id <= 255)) || { echo 'Video numbers must be in 0..255' >&2; exit 1; }
done
labels=(KinectRGB KinectIR KinectDepth)
run() {
    if $dry_run; then printf '%q ' "$@"; printf '\n'; else "$@"; fi
}
if ! $dry_run; then
    for tool in modprobe v4l2loopback-ctl udevadm awk; do
        command -v "$tool" >/dev/null || { echo "Missing $tool (see README Arch dependencies)" >&2; exit 1; }
    done
    [[ $(id -u) == 0 ]] || { echo 'Run this script with sudo' >&2; exit 1; }
fi
# devices=0 avoids an unwanted default camera when first loading the module.
run modprobe v4l2loopback devices=0
missing=()
for i in 0 1 2; do
    device=/dev/video${ids[$i]}
    if ! $dry_run; then
        info=$(v4l2loopback-ctl query "$device" 2>/dev/null || true)
        label=$(awk '/^[[:space:]]*card_label[[:space:]]*:/ {sub(/^[^:]*:[[:space:]]*/, ""); print; exit}' <<< "$info")
        if [[ -n $label ]]; then
            exclusive=$(awk '/^[[:space:]]*announce_all_caps[[:space:]]*:/ {sub(/^[^:]*:[[:space:]]*/, ""); print; exit}' <<< "$info")
            [[ $label == "${labels[$i]}" && $exclusive == 0 ]] || {
                echo "$device is already a different/incompatible loopback; choose free --video-nr values" >&2; exit 1;
            }
            echo "Keeping $device ($label)"
            continue
        fi
        if [[ -e $device || -e /sys/class/video4linux/video${ids[$i]} ]]; then
            echo "$device is occupied by another camera; choose free --video-nr values" >&2; exit 1
        fi
    fi
    missing+=("$i")
done
# Check all collisions first so a conflict never leaves a half-created setup.
for i in "${missing[@]}"; do
    run v4l2loopback-ctl add -n "${labels[$i]}" -x 1 -w 1920 -h 1080 -o 16 "/dev/video${ids[$i]}"
done
run udevadm settle --timeout=10
echo 'Kinect loopbacks ready. Start kinectcam before opening a camera application.'
