#!/bin/sh

nox=$1
dir=$2

if [ -z "$nox" ] || [ -z "$dir" ]; then
    echo "usage: $0 <nox> <golden-dir>" >&2
    exit 2
fi

work=$(mktemp -d) || exit 2
trap 'rm -rf "$work"' EXIT

failures=0

fail()
{
    echo "FAIL $1: $2" >&2
    failures=$((failures + 1))
}

for source in "$dir"/*.nox; do
    name=$(basename "$source" .nox)
    base=${source%.nox}
    exe=$work/$name

    if [ -f "$base.error" ]; then
        if "$nox" -o "$exe" "$source" 2>"$work/$name.err"; then
            fail "$name" "expected a compile error"
        elif ! grep -qF -- "$(cat "$base.error")" "$work/$name.err"; then
            fail "$name" "compile error does not mention '$(cat "$base.error")'"
        fi
        continue
    fi

    if ! "$nox" -o "$exe" "$source" 2>"$work/$name.err"; then
        fail "$name" "compile failed: $(cat "$work/$name.err")"
        continue
    fi

    "$exe" >"$work/$name.out" 2>"$work/$name.rt"
    status=$?

    expected_status=0
    [ -f "$base.status" ] && expected_status=$(cat "$base.status")

    if [ "$status" -ne "$expected_status" ]; then
        fail "$name" "exit status $status, expected $expected_status"
    fi

    expected_out=$base.out
    [ -f "$expected_out" ] || expected_out=/dev/null

    if ! cmp -s "$work/$name.out" "$expected_out"; then
        fail "$name" "stdout differs"
        diff "$expected_out" "$work/$name.out" >&2
    fi
done

[ "$failures" -eq 0 ]
