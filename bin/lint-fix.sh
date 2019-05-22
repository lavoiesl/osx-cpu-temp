#!/bin/sh

if [ ! -f ".clang-format" ]; then
    echo ".clang-format file not found!"
    exit 1
fi

changed=0

for file in $(find . -name '*.c' -o -name '*.h'); do
    tmp="$(mktemp)"
    clang-format -style=file "${file}" > "${tmp}"
    if cmp "${tmp}" "${file}" >/dev/null; then
        rm "${tmp}"
    else
        mv "${tmp}" "${file}"
        echo "Formatted ${file}"
        changed=1
    fi
done

if [[ "${changed}" -eq 0 ]]; then
    echo "Already formatted"
fi
