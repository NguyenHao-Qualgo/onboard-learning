# Make sure we have sbsign available
DEPENDS:append = " sbsigntool-native"

# Sign /boot/Image after it has been installed by l4t-launcher-extlinux
do_install:append() {
    if [ -z "${KERNEL_DB_KEY}" ] || [ -z "${KERNEL_DB_CERT}" ]; then
        bbfatal "l4t-launcher-extlinux: KERNEL_DB_KEY / KERNEL_DB_CERT not set – refusing to leave /boot/Image unsigned"
    fi

    KIMG="${D}/boot/Image"

    if [ ! -f "${KIMG}" ]; then
        bbwarn "l4t-launcher-extlinux: ${KIMG} not found, skipping signing"
        return
    fi

    bbnote "l4t-launcher-extlinux: signing ${KIMG} with ${KERNEL_DB_CERT}"

    sbsign --key  "${KERNEL_DB_KEY}" \
           --cert "${KERNEL_DB_CERT}" \
           --output "${KIMG}.signed" \
           "${KIMG}"

    mv "${KIMG}.signed" "${KIMG}"
}
