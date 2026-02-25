#!/usr/bin/env bash
set -euo pipefail

# ===== CONFIG =====
# Video nodes to test (set explicitly, or leave empty to auto-detect first 6)
VIDEO_NODES=()                 # e.g. (0 1 2 3 4 5)
EXPECTED_COUNT=6

# Streaming test parameters
STREAM_COUNT=30                # frames per device
STREAM_TIMEOUT=5               # seconds (v4l2-ctl --stream-timeout)
PIXELFORMAT=""                 # e.g. YUYV, RG10, BA10 (leave empty = don't force)
WIDTH="1920"                       # e.g. 1920
HEIGHT="1536"                      # e.g. 1080
N_TEST=1000
# ===== HELPERS =====
log() {
    local level="$1"
    shift
    local msg="$*"

    case "$level" in
        INFO)  color="\e[32m" ;;   # Green
        WARN)  color="\e[33m" ;;   # Yellow
        ERROR) color="\e[31m" ;;   # Red
        DEBUG) color="\e[36m" ;;   # Cyan
        *)     color="\e[0m"  ;;   # Default
    esac

    echo -e "${color}[$(date +'%F %T')] [$level] ${msg}\e[0m"
}

detect_nodes() {
  local nodes=()
  for p in /dev/video*; do
    [[ -e "$p" ]] || continue
    nodes+=("${p#/dev/video}")
  done
  printf "%s\n" "${nodes[@]}" | sort -n | head -n "$EXPECTED_COUNT"
}

fmt_args() {
  # Build optional --set-fmt-video argument if user requested format/size forcing.
  if [[ -n "$PIXELFORMAT" && -n "$WIDTH" && -n "$HEIGHT" ]]; then
    echo "--set-fmt-video=width=$WIDTH,height=$HEIGHT,pixelformat=$PIXELFORMAT"
  elif [[ -n "$WIDTH" && -n "$HEIGHT" ]]; then
    echo "--set-fmt-video=width=$WIDTH,height=$HEIGHT"
  elif [[ -n "$PIXELFORMAT" ]]; then
    echo "--set-fmt-video=pixelformat=$PIXELFORMAT"
  else
    echo ""
  fi
}

test_one() {
  local dev="$1"
  local fmt
  fmt="$(fmt_args)"

  log "Testing /dev/video$dev"
  #log "  Current: $(v4l2-ctl -d "$dev" --get-fmt-video | tr '\n' ' ')"
  #log "  Params : $(v4l2-ctl -d "$dev" --get-parm 2>/dev/null | tr '\n' ' ' || echo '(no parm)')"

  # Run a short mmap stream. We treat *any* non-zero exit as failure.
  # --stream-to is avoided to reduce I/O and permissions issues.
  local cmd=(timeout 5s v4l2-ctl -d "$dev")
  if [[ -n "$fmt" ]]; then
    cmd+=("$fmt")
  fi
  cmd+=(--stream-mmap --stream-count="$STREAM_COUNT" --stream-count=30)

  # Capture output for diagnostics
  local out
  if ! out="$("${cmd[@]}" 2>&1)"; then
    echo "$out" >&2
    return 1
  fi

  # Heuristic check: ensure frames were actually captured.
  # v4l2-ctl usually prints "cap" lines or a summary; if nothing, still OK if exit=0.
  if echo "$out" | grep -qiE "error|failed|timeout|VIDIOC_STREAMON|VIDIOC_QBUF|VIDIOC_DQBUF"; then
    echo "$out" >&2
    return 1
  fi

  log INFO "  OK"
  return 0
}

# ===== MAIN =====

probe_fail=0
stream_fail=0

for ((i=0; i<=N_TEST; i++)); do

    echo "-----------------------------------------------"
    log "Reload driver"
    log "Test $i / $N_TEST"
    sudo systemctl restart zed_x_daemon.service

    sleep 1
    echo "-----------------------------------------------"

    if [[ ${#VIDEO_NODES[@]} -eq 0 ]]; then
    mapfile -t VIDEO_NODES < <(detect_nodes)
    fi

    if [[ ${#VIDEO_NODES[@]} -lt $EXPECTED_COUNT ]]; then
    log ERROR "ERROR: expected at least $EXPECTED_COUNT video nodes, found ${#VIDEO_NODES[@]}: ${VIDEO_NODES[*]:-(none)}"
    probe_fail=$((probe_fail+1))
    continue
    fi

    fails=0

    for d in "${VIDEO_NODES[@]}"; do
    if ! test_one "$d"; then
        log ERROR "FAIL: /dev/video$d did not stream correctly"
        fails=$((fails+1))
    fi

    if [[ $fails -gt 0 ]]; then
        stream_fail=$((stream_fail+1))
    fi

    done
    echo ""
    echo ""

done

echo ""
echo ""
echo ""
echo ""
echo "-----------------------------------------------"
echo "RESULT: "
if [[ $stream_fail -eq 0 ]]; then
    log INFO "Cameras stream fails = $stream_fail"
elif [[ $stream_fail -eq $N_TEST ]]; then
    log ERROR "Cameras stream fails = $stream_fail"
else
    log WARN "Cameras stream fails = $stream_fail"
fi

if [[ $probe_fail -eq 0 ]]; then
    log INFO "Probe fails = $probe_fail"
elif [[ $probe_fail -eq $N_TEST ]]; then
    log ERROR "Probe fails = $probe_fail"
else
    log WARN "Probe fails = $probe_fail"
fi