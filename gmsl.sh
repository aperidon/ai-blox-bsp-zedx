#!/bin/sh

count=0
device_ids="0 1 2 3 4 5"
per_device_timeout=10
stream_options="--set-fmt-video=width=1920,height=1536,pixelformat=YUYV"

reboot_opening(){
    local duration_stream=$1
    local sleep_duration=$2
    local id
    local rc
    local ok_count
    local expected_count
    local capture_file
    local log_line
    local pid
    local remaining


    file="$HOME/count.txt"
    file_fail="$HOME/fail_count.txt"

    total_count=0

    if [ ! -f "$file" ]; then
        echo 1 > $file
    else
        count=$(head -n 1 "$file")
    fi

    if [ ! -f "$file_fail" ]; then
        count_fail=1
        echo "$count_fail" > $file_fail
    else
        count_fail=$(head -n 1 "$file_fail")
        echo "fail count: $count_fail"
        count_fail=$((count_fail+1))
        sed -i "1s/.*/$count_fail/" "$file_fail"
    fi 

    echo "reboot count: $count"

    ok_count=0
    expected_count=0
    for id in $device_ids; do
        expected_count=$((expected_count+1))
        capture_file="$HOME/gmsl_capture_reboot_${id}.txt"
        rm -f "$capture_file"

        echo "Opening /dev/video${id}..."
        v4l2-ctl -d"$id" $stream_options --stream-mmap --stream-count="$duration_stream" > $capture_file 2>&1 &
        pid=$!
        remaining=$per_device_timeout
        while [ "$remaining" -gt 0 ] && kill -0 "$pid" 2>/dev/null; do
            sleep 1
            remaining=$((remaining-1))
        done
        if kill -0 "$pid" 2>/dev/null; then
            echo "TIMEOUT /dev/video${id} after ${per_device_timeout}s"
            kill "$pid" 2>/dev/null
            sleep 1
            kill -9 "$pid" 2>/dev/null
            wait "$pid" 2>/dev/null
            rc=124
        else
            wait "$pid"
            rc=$?
        fi

        if [ "$rc" -eq 0 ] && [ -s "$capture_file" ]; then
            first_char=$(dd if="$capture_file" bs=1 count=1 2>/dev/null)
            if [ "x$first_char" = "x<" ]; then
                ok_count=$((ok_count+1))
            else
                log_line="FAILED /dev/video${id} (bad output: does not start with '<', bytes=$(wc -c < $capture_file 2>/dev/null || echo 0))"
                echo "$log_line"
            fi
        elif [ "$rc" -eq 0 ]; then
            echo "FAILED /dev/video${id} (empty output file)"
        else
            log_line="FAILED /dev/video${id} (rc=${rc}, bytes=$(wc -c < "$capture_file" 2>/dev/null || echo 0))"
            echo "$log_line"
        fi

    done

    sleep $sleep_duration

    echo "Streaming OK: ${ok_count}/${expected_count}"

    # Consider success only if all 6 devices streamed successfully.
    if [ "$ok_count" -eq "$expected_count" ]; then
        count=$((count+1))
        sed -i "1s/.*/$count/" "$file"
        echo "reboot count: $count"
        count_fail=$((count_fail-1))
        sed -i "1s/.*/$count_fail/" "$file_fail"
        echo "fail count: $count_fail"
        
    else
        echo "Not all devices streamed; not incrementing reboot count."
    fi
    total_count=$(($count + $count_fail))
    echo "Done"
}

while [ ! -e /dev/video5 ]; do
    sleep 0.5
done

reboot_opening 100 5

if [ $total_count -lt $1 ]; then 
    echo "Reboot ($total_count)"
    reboot
fi
