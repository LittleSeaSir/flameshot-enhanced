#!/usr/bin/env bash
# Build the Fedora 44 RPM and source RPM from a committed source snapshot.
set -euo pipefail

repo_dir=$(git -C "$(dirname "${BASH_SOURCE[0]}")" rev-parse --show-toplevel)
cd "$repo_dir"

if [[ "$(rpm --eval '%{?fedora}')" != 44 || "$(uname -m)" != x86_64 ]]; then
    printf '%s\n' 'This release recipe targets Fedora 44 x86_64.' >&2
    exit 1
fi
if [[ -n "$(git status --porcelain --untracked-files=normal)" ]]; then
    printf '%s\n' 'Commit source changes before packaging so the RPM matches its Git revision.' >&2
    exit 1
fi

release_version=14.0.0-littlesea.1
archive_name="flameshot-enhanced-${release_version}"
rpm_dir=${1:-"$repo_dir/build/rpm"}
mkdir -p "$rpm_dir"
rpm_dir=$(cd "$rpm_dir" && pwd)
mkdir -p "$rpm_dir"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
jobs=${FLAMESHOT_BUILD_JOBS:-2}
if [[ ! "$jobs" =~ ^[1-9][0-9]*$ ]]; then
    printf '%s\n' 'FLAMESHOT_BUILD_JOBS must be a positive integer.' >&2
    exit 1
fi

export SOURCE_DATE_EPOCH
SOURCE_DATE_EPOCH=$(git log -1 --format=%ct)
commit=$(git rev-parse --short=12 HEAD)
git archive --format=tar --prefix="${archive_name}/" HEAD |
    gzip -n > "$rpm_dir/SOURCES/${archive_name}.tar.gz"

rpmbuild --noclean -ba packaging/rpm/flameshot-enhanced.spec \
    --define "_topdir $rpm_dir" \
    --define "commit $commit" \
    --define "_smp_build_ncpus $jobs" \
    --define "_smp_mflags -j$jobs"

printf '\nRPM artifacts:\n'
find "$rpm_dir/RPMS" "$rpm_dir/SRPMS" -type f -name '*.rpm' -print
