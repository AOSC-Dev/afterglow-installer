// Automatically generated file
#include <stdbool.h>
#include <string.h>

const bool is_reserved_user(const char* restrict user) {
    const size_t len = strnlen(user, 32);
    switch (len) {
        default:
           return false;
        case 4:
            if (__builtin_memcmp(user, "_apt", 4) == 0) return true;
            if (__builtin_memcmp(user, "dbus", 4) == 0) return true;
            if (__builtin_memcmp(user, "http", 4) == 0) return true;
            if (__builtin_memcmp(user, "i2pd", 4) == 0) return true;
            if (__builtin_memcmp(user, "ldap", 4) == 0) return true;
            if (__builtin_memcmp(user, "lxdm", 4) == 0) return true;
            if (__builtin_memcmp(user, "qemu", 4) == 0) return true;
            if (__builtin_memcmp(user, "root", 4) == 0) return true;
            if (__builtin_memcmp(user, "sddm", 4) == 0) return true;
            if (__builtin_memcmp(user, "seat", 4) == 0) return true;
            if (__builtin_memcmp(user, "xpra", 4) == 0) return true;
            break;
        case 8:
            if (__builtin_memcmp(user, "adbusers", 8) == 0) return true;
            if (__builtin_memcmp(user, "partimag", 8) == 0) return true;
            if (__builtin_memcmp(user, "postgres", 8) == 0) return true;
            break;
        case 5:
            if (__builtin_memcmp(user, "amule", 5) == 0) return true;
            if (__builtin_memcmp(user, "avahi", 5) == 0) return true;
            if (__builtin_memcmp(user, "boinc", 5) == 0) return true;
            if (__builtin_memcmp(user, "caddy", 5) == 0) return true;
            if (__builtin_memcmp(user, "fcron", 5) == 0) return true;
            if (__builtin_memcmp(user, "mysql", 5) == 0) return true;
            if (__builtin_memcmp(user, "named", 5) == 0) return true;
            if (__builtin_memcmp(user, "proxy", 5) == 0) return true;
            if (__builtin_memcmp(user, "pulse", 5) == 0) return true;
            if (__builtin_memcmp(user, "rtkit", 5) == 0) return true;
            if (__builtin_memcmp(user, "saned", 5) == 0) return true;
            if (__builtin_memcmp(user, "spamd", 5) == 0) return true;
            if (__builtin_memcmp(user, "uuidd", 5) == 0) return true;
            break;
        case 6:
            if (__builtin_memcmp(user, "apache", 6) == 0) return true;
            if (__builtin_memcmp(user, "clamav", 6) == 0) return true;
            if (__builtin_memcmp(user, "colord", 6) == 0) return true;
            if (__builtin_memcmp(user, "docker", 6) == 0) return true;
            if (__builtin_memcmp(user, "locate", 6) == 0) return true;
            if (__builtin_memcmp(user, "netdev", 6) == 0) return true;
            if (__builtin_memcmp(user, "nobody", 6) == 0) return true;
            if (__builtin_memcmp(user, "ocserv", 6) == 0) return true;
            if (__builtin_memcmp(user, "pesign", 6) == 0) return true;
            if (__builtin_memcmp(user, "quagga", 6) == 0) return true;
            if (__builtin_memcmp(user, "usbmux", 6) == 0) return true;
            if (__builtin_memcmp(user, "vboxsf", 6) == 0) return true;
            break;
        case 3:
            if (__builtin_memcmp(user, "bin", 3) == 0) return true;
            if (__builtin_memcmp(user, "mpd", 3) == 0) return true;
            if (__builtin_memcmp(user, "nut", 3) == 0) return true;
            if (__builtin_memcmp(user, "pcp", 3) == 0) return true;
            if (__builtin_memcmp(user, "tor", 3) == 0) return true;
            if (__builtin_memcmp(user, "tss", 3) == 0) return true;
            if (__builtin_memcmp(user, "znc", 3) == 0) return true;
            break;
        case 7:
            if (__builtin_memcmp(user, "dnsmasq", 7) == 0) return true;
            if (__builtin_memcmp(user, "forgejo", 7) == 0) return true;
            if (__builtin_memcmp(user, "greeter", 7) == 0) return true;
            if (__builtin_memcmp(user, "jenkins", 7) == 0) return true;
            if (__builtin_memcmp(user, "lightdm", 7) == 0) return true;
            if (__builtin_memcmp(user, "owntone", 7) == 0) return true;
            if (__builtin_memcmp(user, "plocate", 7) == 0) return true;
            if (__builtin_memcmp(user, "polkitd", 7) == 0) return true;
            if (__builtin_memcmp(user, "privoxy", 7) == 0) return true;
            if (__builtin_memcmp(user, "sanlock", 7) == 0) return true;
            if (__builtin_memcmp(user, "stunnel", 7) == 0) return true;
            if (__builtin_memcmp(user, "unbound", 7) == 0) return true;
            if (__builtin_memcmp(user, "varnish", 7) == 0) return true;
            break;
        case 19:
            if (__builtin_memcmp(user, "gnome-initial-setup", 19) == 0) return true;
            break;
        case 11:
            if (__builtin_memcmp(user, "libvirtdbus", 11) == 0) return true;
            break;
        case 9:
            if (__builtin_memcmp(user, "memcached", 9) == 0) return true;
            if (__builtin_memcmp(user, "syncthing", 9) == 0) return true;
            if (__builtin_memcmp(user, "tinyproxy", 9) == 0) return true;
            break;
        case 13:
            if (__builtin_memcmp(user, "nopasswdlogin", 13) == 0) return true;
            break;
        case 10:
            if (__builtin_memcmp(user, "readymedia", 10) == 0) return true;
            break;
        case 18:
            if (__builtin_memcmp(user, "syncthing-discosrv", 18) == 0) return true;
            if (__builtin_memcmp(user, "syncthing-relaysrv", 18) == 0) return true;
            break;
        case 17:
            if (__builtin_memcmp(user, "systemd-bus-proxy", 17) == 0) return true;
            break;
        case 16:
            if (__builtin_memcmp(user, "systemd-coredump", 16) == 0) return true;
            if (__builtin_memcmp(user, "systemd-timesync", 16) == 0) return true;
            break;
        case 23:
            if (__builtin_memcmp(user, "systemd-journal-gateway", 23) == 0) return true;
            break;
        case 22:
            if (__builtin_memcmp(user, "systemd-journal-remote", 22) == 0) return true;
            if (__builtin_memcmp(user, "systemd-journal-upload", 22) == 0) return true;
            break;
        case 15:
            if (__builtin_memcmp(user, "systemd-network", 15) == 0) return true;
            if (__builtin_memcmp(user, "systemd-resolve", 15) == 0) return true;
            break;
        case 14:
            if (__builtin_memcmp(user, "systemd-nobody", 14) == 0) return true;
            if (__builtin_memcmp(user, "tox-bootstrapd", 14) == 0) return true;
            break;
        case 12:
            if (__builtin_memcmp(user, "transmission", 12) == 0) return true;
            break;
    }
    return false;
}

