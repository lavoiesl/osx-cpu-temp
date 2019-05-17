#!/bin/bash

if [ ! -f ".clang-format" ]; then
    echo ".clang-format file not found!"
    exit 1
fi

changed=0

for file in $(find . -name '*.c' -o -name '*.h'); do
    formatted="$(clang-format -style=file "${file}")"
    if ! echo "${formatted}" | cmp - "${file}" >/dev/null; then
        echo "${formatted}" > "${file}"
        echo "Formatted ${file}"
        changed=1
    fi
done

if [[ "${changed}" -eq 0 ]]; then
    echo "Already formatted"
fi
