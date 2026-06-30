#!/usr/bin/env bash

OUT="output.txt"
> "$OUT"

DIRS=(
"include/scheduler"
"src/scheduler"
"include/thread"
"src/thread"
"tests/scheduler"
)

for dir in "${DIRS[@]}"; do
    echo "==================================================" >> "$OUT"
    echo "DIRECTORY: $dir" >> "$OUT"
    echo "==================================================" >> "$OUT"

    if [ -d "$dir" ]; then
        find "$dir" -type f | sort | while read -r file; do
            echo >> "$OUT"
            echo "--------------------------------------------------" >> "$OUT"
            echo "FILE: $file" >> "$OUT"
            echo "--------------------------------------------------" >> "$OUT"
            cat "$file" >> "$OUT"
            echo >> "$OUT"
        done
    else
        echo "Directory not found." >> "$OUT"
        echo >> "$OUT"
    fi
done

echo "Generated: $OUT"