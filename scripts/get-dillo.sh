#!/bin/bash
# Downloads Alpine 3.21 Dillo browser + deps into rootfs/
set -e
ROOTFS="$(cd "$(dirname "$0")/.." && pwd)/rootfs"
CACHE=/tmp/dillo-pkgs
MAIN="https://dl-cdn.alpinelinux.org/alpine/v3.21/main/x86_64"
COMM="https://dl-cdn.alpinelinux.org/alpine/v3.21/community/x86_64"

MAIN_PKGS=(
    "libjpeg-turbo-3.0.4-r0"
    "libpng-1.6.57-r0"
    "zlib-1.3.2-r0"
    "mbedtls-3.6.6-r0"
    "libstdc%2B%2B-14.2.0-r4"
    "libgcc-14.2.0-r4"
    "cairo-1.18.4-r0"
)

COMM_PKGS=(
    "libfltk-1.3.10-r0"
    "dillo-3.2.0-r0"
)

mkdir -p "$CACHE"

fetch_pkg() {
    local url="$1" pkg="$2"
    local dest="$CACHE/${pkg}.apk"
    if [ ! -f "$dest" ]; then
        echo "  GET $pkg"
        curl -fsSL --max-time 60 --retry 3 "$url/${pkg}.apk" -o "$dest"
    else
        echo "  cached $pkg"
    fi
}

extract_pkg() {
    local pkg="$1"
    tar -xzf "$CACHE/${pkg}.apk" -C "$ROOTFS" \
        --exclude='.PKGINFO' --exclude='.SIGN.*' --exclude='.CHANGELOG' \
        --exclude='usr/include' \
        --exclude='usr/share/doc' --exclude='usr/share/man' \
        --exclude='usr/lib/pkgconfig' --exclude='usr/lib/cmake' \
        2>/dev/null || true
}

echo "=== Fetching Dillo dependencies (main) ==="
for p in "${MAIN_PKGS[@]}"; do fetch_pkg "$MAIN" "$p"; done

echo "=== Fetching Dillo + fltk (community) ==="
for p in "${COMM_PKGS[@]}"; do fetch_pkg "$COMM" "$p"; done

echo "=== Extracting to rootfs ==="
for p in "${MAIN_PKGS[@]}"; do extract_pkg "$p"; done
for p in "${COMM_PKGS[@]}"; do extract_pkg "$p"; done

echo "=== Creating /etc/dillo/ config ==="
mkdir -p "$ROOTFS/etc/dillo"
# Minimal dillorc: disable SSL cert verification (no CA store in rootfs)
cat > "$ROOTFS/etc/dillo/dillorc" <<'EOF'
# Dillo config for Kyronix
http_proxy_host=""
http_proxy_port=0
http_user_agent="Dillo/3.2.0"
EOF

mkdir -p "$ROOTFS/root/.dillo"
cat > "$ROOTFS/root/.dillo/dillorc" <<'EOF'
start_page="about:splash"
EOF

echo "=== Checking dillo binary ==="
if [ -f "$ROOTFS/usr/bin/dillo" ]; then
    echo "  OK: /usr/bin/dillo"
    file "$ROOTFS/usr/bin/dillo" 2>/dev/null || true
else
    echo "  WARN: dillo not found at usr/bin/dillo"
    find "$ROOTFS" -name "dillo" -type f 2>/dev/null || true
fi

echo "=== Done. Run 'dillo http://example.com' inside Xorg. ==="
