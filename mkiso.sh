#!/bin/bash -e

DRACUT_INSTALL="${DRACUT_INSTALL:-/usr/lib/dracut/dracut-install}"
GRUB_MKRESCUE="${GRUB_MKRESCUE:-grub-mkrescue}"
TARBALL_PATH="$1"

if [ -z "$TARBALL_PATH" ]; then
    echo "Usage: $0 <tarball>"
    exit 1
fi

TMPDIR="$(mktemp -d -p "$PWD")"

cleanup() {
    rm -rf "$TMPDIR"
}
trap cleanup EXIT ERR

mkdir -p "$TMPDIR"/sysroot "$TMPDIR"/iso
tar -xf "$TARBALL_PATH" -C "$TMPDIR"/sysroot
install -Dvm755 ./setup "$TMPDIR"/sysroot/usr/bin/setup

wget 'https://repo.aosc.io/debs/pool/stable/main/b/busybox_1.38.0-1_i486.deb' -O "$TMPDIR"/busybox.deb
dpkg-deb -x "$TMPDIR"/busybox.deb "$TMPDIR"/sysroot

FILES_TO_INSTALL=(
    /usr/bin/setup
    /usr/bin/mkfs.ext4
    /usr/bin/mkfs.vfat
    /usr/bin/fdisk
    /usr/bin/cfdisk
    /usr/bin/lsblk
    /usr/bin/bash
    /usr/bin/busybox
    /usr/bin/nano
)

for i in "${FILES_TO_INSTALL[@]}"; do
    "$DRACUT_INSTALL" -l "$i" -r "$TMPDIR"/sysroot -D "$TMPDIR"/iso -v
done

cp -ar "$TMPDIR"/sysroot/boot "$TMPDIR"/iso/boot
mv -v "$TMPDIR"/iso/boot/vmlinuz-* "$TMPDIR"/iso/boot/vmlinuz
mkdir -pv "$TMPDIR"/iso/usr/lib "$TMPDIR"/iso/usr/share
cp -ar "$TMPDIR"/sysroot/usr/lib/modules "$TMPDIR"/iso/usr/lib/modules
cp -av "$TMPDIR"/sysroot/usr/share/zoneinfo "$TMPDIR"/iso/usr/share/zoneinfo

fakeroot "$TMPDIR"/iso/usr/bin/busybox --install -s "$TMPDIR"/iso/usr/bin

mkdir -pv "$TMPDIR"/iso/var/cache/system-images/ "$TMPDIR"/iso/{boot,dev,etc,mnt,proc,run,sys,tmp,usr}
TARBALL_INSTALL_NAME="${TMPDIR}/iso/var/cache/system-images/base.tar${TARBALL_PATH#*.tar}"
install -Dvm644 "$TARBALL_PATH" "$TARBALL_INSTALL_NAME"
pushd "$(dirname "$TARBALL_INSTALL_NAME")"
sha256sum "$(basename "$TARBALL_INSTALL_NAME")" > "$(basename "$TARBALL_INSTALL_NAME")".sha256sum
popd
"$GRUB_MKRESCUE" -d "$TMPDIR"/sysroot/usr/lib/grub/i386-pc -o boot.iso "$TMPDIR"/iso
rm -rf "$TMPDIR"/sysroot
