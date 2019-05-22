#!/bin/sh

if [ ! -f ".clang-format" ]; then
    echo ".clang-format file not found!"
    exit 1
fi

failures=0
for file in $(find . -name '*.c' -o -name '*.h'); do
    echo "Checking ${file}"
    tmp="$(mktemp)"
    clang-format -style=file "${file}" > "${tmp}"
    diff -u "${tmp}" "${file}"
    if [[ $? -gt 0 ]]; then
        failures=$((failures + 1))
    fi
    rm "${tmp}"
done

[[ "${failures}" -eq 0 ]] || exit 1
