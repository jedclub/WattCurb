#include "policy/mitigation_engine.hpp"
#include "policy/memory_pressure_guard.hpp"
#include "policy/process_classifier.hpp"
#include "policy/battery_feature.hpp"
#include "core/posix_fs.hpp"
#include "core/scoped_profiler.hpp"
#include "core/event_logger.hpp"
#include <sched.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/netlink.h>
#include <linux/genetlink.h>
#include <linux/nl80211.h>

namespace wattcurb::policy {

namespace {

#ifndef IOPRIO_CLASS_IDLE
#define IOPRIO_CLASS_IDLE 3
#endif

#ifndef IOPRIO_CLASS_BE
#define IOPRIO_CLASS_BE 2
#endif

#ifndef IOPRIO_WHO_PROCESS
#define IOPRIO_WHO_PROCESS 1
#endif

#ifndef IOPRIO_PRIO_VALUE
#define IOPRIO_PRIO_VALUE(class_val, data_val) (((class_val) << 13) | ((data_val) & 0x1fff))
#endif

// REF-REQ-055: Hardware baseline state recorded at bootstrap
static MitigationEngine::HardwareBaselineState s_hardware_baseline{};

// ---------------------------------------------------------------------------
// Hardware Actuation Sandbox (REF-REQ-092, REF-ARCH-069)
//
// Every actuator that mutates live kernel, sysfs or process state funnels
// through the wrappers below. When the sandbox is engaged they perform no
// syscall and report failure, which is exactly how they already behave for an
// unprivileged caller. The Oracle Gate engages it so the suite can exercise
// policy logic without clamping the host CPU to C0, rewriting NVMe APST, or
// renicing the developer's audio daemon.
//
// Production default is OFF; the branch is a single predictable load on a
// syscall-bound path, never on the monitoring hot loop.
//
// REF-REQ-092: a non-interactive CLI run (the PGO training workloads in
// scripts/build_pgo.sh, CI, benchmarking) must also be able to guarantee it
// never touches live hardware. WATTCURB_ACTUATION_SANDBOX=1 engages the sandbox
// at startup so those runs have no path to /sys, /proc/sys or another process.
static bool s_actuation_sandbox = []() noexcept {
    const char* env = ::getenv("WATTCURB_ACTUATION_SANDBOX");
    return env != nullptr && env[0] != '\0' && env[0] != '0';
}();

// REF-REQ-096: Last observed PCM playback state, refreshed once per cycle.
static MitigationEngine::AudioStreamState s_audio_state{};

// REF-REQ-099: Profile in force, published each cycle by the policy driver.
static PowerProfileMode s_effective_profile = PowerProfileMode::Balanced;

// REF-REQ-104: Whether the last EPP write actually reached a cpufreq policy.
// The status summary must not claim a CPU EPP setting the host does not expose.
static bool s_epp_applied = false;

[[nodiscard]] inline int hw_open_write(const char* path, int flags) noexcept {
    if (s_actuation_sandbox) return -1;
    return ::open(path, flags);
}

inline int hw_setpriority(int which, id_t who, int prio) noexcept {
    if (s_actuation_sandbox) return -1;
    return ::setpriority(which, who, prio);
}

inline int hw_sched_setaffinity(pid_t pid, size_t size, const cpu_set_t* set) noexcept {
    if (s_actuation_sandbox) return -1;
    return ::sched_setaffinity(pid, size, set);
}

inline int hw_sched_setscheduler(pid_t pid, int policy, const struct sched_param* param) noexcept {
    if (s_actuation_sandbox) return -1;
    return ::sched_setscheduler(pid, policy, param);
}

inline int hw_ioprio_set(int which, int who, int prio) noexcept {
    if (s_actuation_sandbox) return -1;
    return static_cast<int>(::syscall(SYS_ioprio_set, which, who, prio));
}

// Returns 0 (the shell's own result for the backgrounded "... &" commands used
// here) so callers observe the same outcome they would in production, while no
// process is created. Matches the existing WATTCURB_TEST_MOCK_DESKTOP idiom in
// execute_user_desktop_cmd(), which likewise reports success.
inline int hw_system(const char* cmd) noexcept {
    if (s_actuation_sandbox) return 0;
    return ::system(cmd);
}


// ---------------------------------------------------------------------------
// REF-REQ-092 & REF-ARCH-069: Zero-fork wireless control primitives.
//
// The Wi-Fi power-save actuator previously shelled out to `iw` via ::system(),
// measured at ~4.2 ms per call (fork + exec of /bin/sh). A single call consumed
// 84% of the < 5 ms unified rapid-rollback Oracle Gate budget (REF-TEST-020)
// and violated the zero-allocation / zero-fork daemon doctrine (AGENTS.md Sec 9).
// It is replaced below by direct generic-netlink (nl80211) transactions issued
// from stack buffers: no heap, no fork, ~50 us per round trip.
// ---------------------------------------------------------------------------

constexpr size_t NL_MSG_CAPACITY = 512;
constexpr size_t NL_REPLY_CAPACITY = 2048;

// Local 4-byte alignment helper. The kernel NLA_ALIGN/NLA_HDRLEN/NLMSG_ALIGN
// macros fold a signed ~(NLA_ALIGNTO - 1) into size_t arithmetic and trip
// -Wsign-conversion, so the daemon computes netlink alignment itself.
[[nodiscard]] constexpr size_t nl_align(size_t len) noexcept {
    return (len + 3u) & ~static_cast<size_t>(3u);
}

constexpr size_t NL_ATTR_HDRLEN = nl_align(sizeof(nlattr));

// Locate the first wireless interface under /sys/class/net. Shared by the
// power-save and TX-power actuators so hosts whose interface is not literally
// named "wlan0" (wlp1s0, wlp2s0, wlp3s0, ...) are handled uniformly, as
// REQ-092.5 specifies with `iw dev <iface>`.
[[nodiscard]] bool detect_wireless_ifname(char* out, size_t cap) noexcept {
    if (!out || cap == 0) return false;
    out[0] = '\0';

    DIR* dir = ::opendir("/sys/class/net");
    if (!dir) return false;

    bool found = false;
    struct dirent* entry = nullptr;
    while ((entry = ::readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        char wire_path[128];
        std::snprintf(wire_path, sizeof(wire_path), "/sys/class/net/%s/wireless", entry->d_name);
        if (::access(wire_path, F_OK) == 0) {
            std::strncpy(out, entry->d_name, cap - 1);
            out[cap - 1] = '\0';
            found = true;
            break;
        }
    }
    ::closedir(dir);
    return found;
}

// Append one attribute to an in-progress netlink message held on the stack.
[[nodiscard]] bool nl_append_attr(nlmsghdr* nlh, size_t cap, uint16_t type,
                                  const void* data, uint16_t payload_len) noexcept {
    const size_t offset = nl_align(nlh->nlmsg_len);
    const size_t attr_len = static_cast<size_t>(NL_ATTR_HDRLEN) + payload_len;
    if (offset + nl_align(attr_len) > cap) return false;

    char* base = reinterpret_cast<char*>(nlh) + offset;
    nlattr attr{};
    attr.nla_type = type;
    attr.nla_len = static_cast<uint16_t>(attr_len);
    std::memcpy(base, &attr, sizeof(attr));
    std::memcpy(base + NL_ATTR_HDRLEN, data, payload_len);

    const size_t pad = nl_align(attr_len) - attr_len;
    if (pad > 0) std::memset(base + attr_len, 0, pad);

    nlh->nlmsg_len = static_cast<uint32_t>(offset + nl_align(attr_len));
    return true;
}

// Walk a generic-netlink payload and copy out one fixed-width attribute value.
[[nodiscard]] bool nl_find_attr(const char* payload, const char* end, uint16_t type,
                                void* out, size_t out_len) noexcept {
    const char* p = payload;
    while (p + NL_ATTR_HDRLEN <= end) {
        nlattr attr{};
        std::memcpy(&attr, p, sizeof(attr));
        if (static_cast<size_t>(attr.nla_len) < NL_ATTR_HDRLEN) break;
        if (p + attr.nla_len > end) break;
        if (attr.nla_type == type &&
            static_cast<size_t>(attr.nla_len) >= static_cast<size_t>(NL_ATTR_HDRLEN) + out_len) {
            std::memcpy(out, p + NL_ATTR_HDRLEN, out_len);
            return true;
        }
        p += nl_align(static_cast<size_t>(attr.nla_len));
    }
    return false;
}

// Generic-netlink socket with a bounded receive timeout, so a wedged kernel
// socket can never stall the rapid-rollback path.
[[nodiscard]] int nl_open_generic() noexcept {
    int fd = ::socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_GENERIC);
    if (fd < 0) return -1;

    sockaddr_nl local{};
    local.nl_family = AF_NETLINK;
    if (::bind(fd, reinterpret_cast<sockaddr*>(&local), static_cast<socklen_t>(sizeof(local))) < 0) {
        ::close(fd);
        return -1;
    }

    timeval tv{};
    tv.tv_usec = 100000; // 100 ms hard ceiling; the kernel answers in microseconds
    (void)::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, static_cast<socklen_t>(sizeof(tv)));
    return fd;
}

// Send one request, read the first reply datagram. Returns bytes read, or -1.
[[nodiscard]] ssize_t nl_transact(int fd, nlmsghdr* req, char* reply, size_t reply_cap) noexcept {
    sockaddr_nl kernel{};
    kernel.nl_family = AF_NETLINK;

    iovec tx_iov{req, req->nlmsg_len};
    msghdr tx{};
    tx.msg_name = &kernel;
    tx.msg_namelen = static_cast<socklen_t>(sizeof(kernel));
    tx.msg_iov = &tx_iov;
    tx.msg_iovlen = 1;
    if (::sendmsg(fd, &tx, 0) < 0) return -1;

    iovec rx_iov{reply, reply_cap};
    msghdr rx{};
    rx.msg_name = &kernel;
    rx.msg_namelen = static_cast<socklen_t>(sizeof(kernel));
    rx.msg_iov = &rx_iov;
    rx.msg_iovlen = 1;
    return ::recvmsg(fd, &rx, 0);
}

// Resolve (and cache) the dynamically assigned nl80211 family id.
[[nodiscard]] uint16_t nl80211_family_id(int fd) noexcept {
    static uint16_t s_family = 0;
    if (s_family != 0) return s_family;

    alignas(4) char buf[NL_MSG_CAPACITY]{};
    auto* nlh = reinterpret_cast<nlmsghdr*>(buf);
    nlh->nlmsg_len = static_cast<uint32_t>(NLMSG_LENGTH(GENL_HDRLEN));
    nlh->nlmsg_type = static_cast<uint16_t>(GENL_ID_CTRL);
    nlh->nlmsg_flags = static_cast<uint16_t>(NLM_F_REQUEST);
    nlh->nlmsg_seq = 1;

    auto* genl = static_cast<genlmsghdr*>(NLMSG_DATA(nlh));
    genl->cmd = CTRL_CMD_GETFAMILY;
    genl->version = 1;

    const auto name_len = static_cast<uint16_t>(std::strlen(NL80211_GENL_NAME) + 1);
    if (!nl_append_attr(nlh, sizeof(buf), static_cast<uint16_t>(CTRL_ATTR_FAMILY_NAME),
                        NL80211_GENL_NAME, name_len)) {
        return 0;
    }

    alignas(4) char reply[NL_REPLY_CAPACITY];
    const ssize_t rlen = nl_transact(fd, nlh, reply, sizeof(reply));
    if (rlen < static_cast<ssize_t>(NLMSG_HDRLEN)) return 0;

    auto* rh = reinterpret_cast<nlmsghdr*>(reply);
    if (rh->nlmsg_type == NLMSG_ERROR) return 0;
    if (rh->nlmsg_len > static_cast<uint32_t>(rlen)) return 0;

    const char* payload = static_cast<const char*>(NLMSG_DATA(rh)) + GENL_HDRLEN;
    const char* end = reply + rh->nlmsg_len;

    uint16_t id = 0;
    if (!nl_find_attr(payload, end, static_cast<uint16_t>(CTRL_ATTR_FAMILY_ID), &id, sizeof(id))) {
        return 0;
    }
    s_family = id;
    return id;
}

// Fill the shared nl80211 request preamble (genl header + NL80211_ATTR_IFINDEX).
[[nodiscard]] bool nl80211_build(nlmsghdr* nlh, size_t cap, uint16_t family, uint8_t cmd,
                                 uint16_t extra_flags, uint32_t ifindex) noexcept {
    nlh->nlmsg_len = static_cast<uint32_t>(NLMSG_LENGTH(GENL_HDRLEN));
    nlh->nlmsg_type = family;
    nlh->nlmsg_flags = static_cast<uint16_t>(static_cast<unsigned>(NLM_F_REQUEST) | extra_flags);
    nlh->nlmsg_seq = 2;

    auto* genl = static_cast<genlmsghdr*>(NLMSG_DATA(nlh));
    genl->cmd = cmd;
    genl->version = 0;

    return nl_append_attr(nlh, cap, static_cast<uint16_t>(NL80211_ATTR_IFINDEX),
                          &ifindex, sizeof(ifindex));
}

// REF-REQ-092.5: NL80211_CMD_SET_POWER_SAVE. Replaces `iw dev <iface> set power_save`.
[[nodiscard]] bool nl80211_set_power_save(const char* ifname, bool enable) noexcept {
    if (s_actuation_sandbox) return false;
    const unsigned int ifindex = ::if_nametoindex(ifname);
    if (ifindex == 0) return false;

    const int fd = nl_open_generic();
    if (fd < 0) return false;

    const uint16_t family = nl80211_family_id(fd);
    if (family == 0) {
        ::close(fd);
        return false;
    }

    alignas(4) char buf[NL_MSG_CAPACITY]{};
    auto* nlh = reinterpret_cast<nlmsghdr*>(buf);
    if (!nl80211_build(nlh, sizeof(buf), family,
                       static_cast<uint8_t>(NL80211_CMD_SET_POWER_SAVE),
                       static_cast<uint16_t>(NLM_F_ACK), ifindex)) {
        ::close(fd);
        return false;
    }

    const uint32_t ps_state = enable ? static_cast<uint32_t>(NL80211_PS_ENABLED)
                                     : static_cast<uint32_t>(NL80211_PS_DISABLED);
    if (!nl_append_attr(nlh, sizeof(buf), static_cast<uint16_t>(NL80211_ATTR_PS_STATE),
                        &ps_state, sizeof(ps_state))) {
        ::close(fd);
        return false;
    }

    alignas(4) char reply[NL_REPLY_CAPACITY];
    const ssize_t rlen = nl_transact(fd, nlh, reply, sizeof(reply));
    ::close(fd);
    if (rlen < static_cast<ssize_t>(NLMSG_HDRLEN)) return false;

    // An ACK is delivered as NLMSG_ERROR carrying error == 0.
    auto* rh = reinterpret_cast<nlmsghdr*>(reply);
    if (rh->nlmsg_type != NLMSG_ERROR) return false;
    if (rh->nlmsg_len < static_cast<uint32_t>(NLMSG_HDRLEN) + sizeof(nlmsgerr)) return false;

    nlmsgerr err{};
    std::memcpy(&err, NLMSG_DATA(rh), sizeof(err));
    return err.error == 0;
}

// REF-REQ-063: NL80211_CMD_SET_WIPHY transmit-power control.
//
// Replaces `iw dev <ifname> set txpower ...` routed through ::system(). That
// construction was a command-injection primitive: ifname comes from a readdir()
// of /sys/class/net, and the kernel's dev_valid_name() rejects only whitespace
// and '/', so ';', '$()', '`', '|', '&' and '>' are all legal interface-name
// characters. An interface named e.g. "wlan0;cmd" yielded
//   iw dev wlan0;cmd set txpower limit 1200 >/dev/null 2>&1 &
// executed by /bin/sh as root. Naming an interface needs CAP_NET_ADMIN, so this
// was defence-in-depth rather than a live escalation - but a root daemon must
// not build shell commands out of names it read off the filesystem at all.
[[nodiscard]] bool nl80211_set_tx_power(const char* ifname, bool automatic, uint32_t mbm) noexcept {
    const unsigned int ifindex = ::if_nametoindex(ifname);
    if (ifindex == 0) return false;

    const int fd = nl_open_generic();
    if (fd < 0) return false;

    const uint16_t family = nl80211_family_id(fd);
    if (family == 0) {
        ::close(fd);
        return false;
    }

    alignas(4) char buf[NL_MSG_CAPACITY]{};
    auto* nlh = reinterpret_cast<nlmsghdr*>(buf);
    if (!nl80211_build(nlh, sizeof(buf), family,
                       static_cast<uint8_t>(NL80211_CMD_SET_WIPHY),
                       static_cast<uint16_t>(NLM_F_ACK), ifindex)) {
        ::close(fd);
        return false;
    }

    const uint32_t setting = automatic ? static_cast<uint32_t>(NL80211_TX_POWER_AUTOMATIC)
                                       : static_cast<uint32_t>(NL80211_TX_POWER_LIMITED);
    bool built = nl_append_attr(nlh, sizeof(buf),
                                static_cast<uint16_t>(NL80211_ATTR_WIPHY_TX_POWER_SETTING),
                                &setting, sizeof(setting));
    if (built && !automatic) {
        built = nl_append_attr(nlh, sizeof(buf),
                               static_cast<uint16_t>(NL80211_ATTR_WIPHY_TX_POWER_LEVEL),
                               &mbm, sizeof(mbm));
    }
    if (!built) {
        ::close(fd);
        return false;
    }

    alignas(4) char reply[NL_REPLY_CAPACITY];
    const ssize_t rlen = nl_transact(fd, nlh, reply, sizeof(reply));
    ::close(fd);
    if (rlen < static_cast<ssize_t>(NLMSG_HDRLEN)) return false;

    auto* rh = reinterpret_cast<nlmsghdr*>(reply);
    if (rh->nlmsg_type != NLMSG_ERROR) return false;
    if (rh->nlmsg_len < static_cast<uint32_t>(NLMSG_HDRLEN) + sizeof(nlmsgerr)) return false;

    nlmsgerr err{};
    std::memcpy(&err, NLMSG_DATA(rh), sizeof(err));
    return err.error == 0;
}

// REF-REQ-092.5: NL80211_CMD_GET_POWER_SAVE (permitted to unprivileged callers).
// Captures the pre-actuation power-save state so demotion restores what the user
// actually had, rather than blindly forcing power save back on.
[[nodiscard]] bool nl80211_get_power_save(const char* ifname, bool* out_enabled) noexcept {
    if (!out_enabled) return false;

    const unsigned int ifindex = ::if_nametoindex(ifname);
    if (ifindex == 0) return false;

    const int fd = nl_open_generic();
    if (fd < 0) return false;

    const uint16_t family = nl80211_family_id(fd);
    if (family == 0) {
        ::close(fd);
        return false;
    }

    alignas(4) char buf[NL_MSG_CAPACITY]{};
    auto* nlh = reinterpret_cast<nlmsghdr*>(buf);
    if (!nl80211_build(nlh, sizeof(buf), family,
                       static_cast<uint8_t>(NL80211_CMD_GET_POWER_SAVE), 0, ifindex)) {
        ::close(fd);
        return false;
    }

    alignas(4) char reply[NL_REPLY_CAPACITY];
    const ssize_t rlen = nl_transact(fd, nlh, reply, sizeof(reply));
    ::close(fd);
    if (rlen < static_cast<ssize_t>(NLMSG_HDRLEN)) return false;

    auto* rh = reinterpret_cast<nlmsghdr*>(reply);
    if (rh->nlmsg_type == NLMSG_ERROR) return false;
    if (rh->nlmsg_len > static_cast<uint32_t>(rlen)) return false;

    const char* payload = static_cast<const char*>(NLMSG_DATA(rh)) + GENL_HDRLEN;
    const char* end = reply + rh->nlmsg_len;

    uint32_t ps_state = 0;
    if (!nl_find_attr(payload, end, static_cast<uint16_t>(NL80211_ATTR_PS_STATE),
                      &ps_state, sizeof(ps_state))) {
        return false;
    }
    *out_enabled = (ps_state == static_cast<uint32_t>(NL80211_PS_ENABLED));
    return true;
}

// REF-REQ-092.3: Parse the active ("*"-marked) row of the amdgpu
// pp_power_profile_mode table, so demotion can restore the real pre-actuation
// mode instead of a hardcoded 0.
[[nodiscard]] int read_gpu_power_profile_mode() noexcept {
    static const char* const paths[] = {
        "/sys/class/drm/card1/device/pp_power_profile_mode",
        "/sys/class/drm/card0/device/pp_power_profile_mode"
    };

    char buf[4096];
    for (const char* path : paths) {
        size_t n = 0;
        if (!core::fs::read_small_file(path, buf, sizeof(buf), &n) || n == 0) continue;

        // Rows look like "  1   3D_FULL_SCREEN*:" - '*' marks the active mode.
        const char* line = buf;
        while (line && *line) {
            const char* nl = std::strchr(line, '\n');
            const size_t len = nl ? static_cast<size_t>(nl - line) : std::strlen(line);
            if (std::memchr(line, '*', len) != nullptr) {
                return static_cast<int>(std::strtol(line, nullptr, 10));
            }
            line = nl ? nl + 1 : nullptr;
        }
    }
    return -1;
}

} // anonymous namespace

MitigationEngine::MitigationEngine() noexcept {
    // Implements REF-REQ-049: Instant immunity audit and self-healing at daemon startup
    audit_and_heal_audio_stack();
    if (!s_hardware_baseline.captured) {
        capture_hardware_baseline();
    }
}

// REF-REQ-115: defined with the SMU actuators below; forward-declared for the
// bootstrap capture, which runs before it in the file.
static const char* find_ryzenadj() noexcept;

void MitigationEngine::capture_hardware_baseline() noexcept {
    if (s_hardware_baseline.captured) {
        return;
    }

    // 1. /sys/firmware/acpi/platform_profile
    char buf[128]{};
    size_t n = 0;
    if (core::fs::read_small_file("/sys/firmware/acpi/platform_profile", buf, sizeof(buf) - 1, &n) && n > 0) {
        while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r' || buf[n - 1] == ' ')) buf[--n] = '\0';
        std::strncpy(s_hardware_baseline.platform_profile, buf, sizeof(s_hardware_baseline.platform_profile) - 1);
    } else {
        std::strncpy(s_hardware_baseline.platform_profile, "balanced", sizeof(s_hardware_baseline.platform_profile) - 1);
    }

    // 2. /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
    n = 0;
    std::memset(buf, 0, sizeof(buf));
    if (core::fs::read_small_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", buf, sizeof(buf) - 1, &n) && n > 0) {
        while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r' || buf[n - 1] == ' ')) buf[--n] = '\0';
        std::strncpy(s_hardware_baseline.cpu_governor, buf, sizeof(s_hardware_baseline.cpu_governor) - 1);
    } else {
        std::strncpy(s_hardware_baseline.cpu_governor, "schedutil", sizeof(s_hardware_baseline.cpu_governor) - 1);
    }

    // 3. /sys/devices/system/cpu/cpufreq/boost
    n = 0;
    std::memset(buf, 0, sizeof(buf));
    if (core::fs::read_small_file("/sys/devices/system/cpu/cpufreq/boost", buf, sizeof(buf) - 1, &n) && n > 0) {
        s_hardware_baseline.cpu_boost = (buf[0] == '1' ? 1 : 0);
    } else {
        s_hardware_baseline.cpu_boost = 1;
    }

    // 4. /sys/module/pcie_aspm/parameters/policy
    // Format: "default [performance] powersave powersupersave"
    n = 0;
    std::memset(buf, 0, sizeof(buf));
    if (core::fs::read_small_file("/sys/module/pcie_aspm/parameters/policy", buf, sizeof(buf) - 1, &n) && n > 0) {
        const char* lbracket = std::strchr(buf, '[');
        const char* rbracket = std::strchr(buf, ']');
        if (lbracket && rbracket && rbracket > lbracket + 1) {
            size_t plen = static_cast<size_t>(rbracket - lbracket - 1);
            plen = std::min(plen, sizeof(s_hardware_baseline.aspm_policy) - 1);
            std::memcpy(s_hardware_baseline.aspm_policy, lbracket + 1, plen);
            s_hardware_baseline.aspm_policy[plen] = '\0';
        } else {
            std::strncpy(s_hardware_baseline.aspm_policy, "default", sizeof(s_hardware_baseline.aspm_policy) - 1);
        }
    } else {
        std::strncpy(s_hardware_baseline.aspm_policy, "default", sizeof(s_hardware_baseline.aspm_policy) - 1);
    }

    // 5. /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq
    //
    // REF-REQ-112: the driver's ceiling is captured FIRST, because the observed
    // scaling_max_freq cannot be trusted as "the user's baseline". A daemon that
    // exits uncleanly in PowerSaver (1.7 GHz cap) or UltraEndurance (1.4 GHz cap)
    // leaves that cap in sysfs - it is global hardware state, not process state,
    // so it survives the exit. The next bootstrap then captures WattCurb's own
    // leftover as the baseline, and Performance and Balanced spend the rest of
    // the machine's life "restoring" a cap that no user ever asked for. Because
    // install.sh restarts the service on every release, this laundering happens
    // routinely rather than exceptionally.
    n = 0;
    std::memset(buf, 0, sizeof(buf));
    if (core::fs::read_small_file("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", buf, sizeof(buf) - 1, &n) && n > 0) {
        const long hw_max = std::strtol(buf, nullptr, 10);
        s_hardware_baseline.hw_max_freq_khz = (hw_max > 0) ? static_cast<uint32_t>(hw_max) : 0;
    } else {
        s_hardware_baseline.hw_max_freq_khz = 0;
    }

    n = 0;
    std::memset(buf, 0, sizeof(buf));
    if (core::fs::read_small_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq", buf, sizeof(buf) - 1, &n) && n > 0) {
        long f = std::strtol(buf, nullptr, 10);
        s_hardware_baseline.scaling_max_freq_khz = (f > 0) ? static_cast<uint32_t>(f) : 1700000;
    } else {
        s_hardware_baseline.scaling_max_freq_khz = 1700000;
    }

    // A ceiling below the driver's own maximum is repaired rather than recorded.
    // This does raise a static third-party cap (a boot-time TLP setting, say)
    // that WattCurb did not impose; that is the deliberate trade. A LIVE
    // competing manager is handled separately by REF-REQ-109, which refuses to
    // fight it for the knobs it holds.
    if (s_hardware_baseline.hw_max_freq_khz > 0 &&
        s_hardware_baseline.scaling_max_freq_khz < s_hardware_baseline.hw_max_freq_khz) {
        char detail[192];
        std::snprintf(detail, sizeof(detail),
                      "Orphaned CPU frequency cap found at bootstrap: scaling_max_freq=%u kHz "
                      "below hardware ceiling %u kHz. Treating the ceiling as the baseline "
                      "(REF-REQ-112).",
                      s_hardware_baseline.scaling_max_freq_khz,
                      s_hardware_baseline.hw_max_freq_khz);
        core::EventLogger::log_alert("REPAIR", detail);
        s_hardware_baseline.scaling_max_freq_khz = s_hardware_baseline.hw_max_freq_khz;
        s_hardware_baseline.orphaned_freq_cap_repaired = true;
    }

    // The boost bit is global hardware state too, and PowerSaver/UltraEndurance
    // clear it. If it is found clear while the ceiling was also found capped,
    // both came from the same orphaned actuation, so the boost baseline is
    // repaired with it. A clear boost bit on an OTHERWISE unrestricted machine
    // is left alone - that one plausibly is the user's own setting.
    if (s_hardware_baseline.orphaned_freq_cap_repaired && s_hardware_baseline.cpu_boost == 0) {
        s_hardware_baseline.cpu_boost = 1;
    }

    // 6. /sys/class/drm/card1-eDP-1/amdgpu/panel_power_savings or card0
    n = 0;
    std::memset(buf, 0, sizeof(buf));
    if (core::fs::read_small_file("/sys/class/drm/card1-eDP-1/amdgpu/panel_power_savings", buf, sizeof(buf) - 1, &n) && n > 0) {
        s_hardware_baseline.panel_power_savings = static_cast<uint32_t>(std::strtoul(buf, nullptr, 10));
    } else if (core::fs::read_small_file("/sys/class/drm/card0-eDP-1/amdgpu/panel_power_savings", buf, sizeof(buf) - 1, &n) && n > 0) {
        s_hardware_baseline.panel_power_savings = static_cast<uint32_t>(std::strtoul(buf, nullptr, 10));
    } else {
        s_hardware_baseline.panel_power_savings = 1;
    }

    // 7. /sys/class/drm/card1/device/power_dpm_force_performance_level or card0
    const char* const gpu_dirs[] = {
        "/sys/class/drm/card1/device",
        "/sys/class/drm/card0/device"
    };
    for (const char* dir : gpu_dirs) {
        char path[128];
        std::snprintf(path, sizeof(path), "%s/power_dpm_force_performance_level", dir);
        n = 0;
        std::memset(buf, 0, sizeof(buf));
        if (core::fs::read_small_file(path, buf, sizeof(buf) - 1, &n) && n > 0) {
            buf[n] = '\0';
            while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == ' ' || buf[n - 1] == '\r')) {
                buf[--n] = '\0';
            }
            std::strncpy(s_hardware_baseline.gpu_dpm_level, buf, sizeof(s_hardware_baseline.gpu_dpm_level) - 1);
            break;
        }
    }

    // 8. SMT (Simultaneous Multithreading / Hyper-Threading) State (REF-REQ-063)
    n = 0;
    std::memset(buf, 0, sizeof(buf));
    if (core::fs::read_small_file("/sys/devices/system/cpu/smt/control", buf, sizeof(buf) - 1, &n) && n > 0) {
        while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == ' ' || buf[n - 1] == '\r')) buf[--n] = '\0';
        std::strncpy(s_hardware_baseline.smt_control, buf, sizeof(s_hardware_baseline.smt_control) - 1);
    } else {
        std::strncpy(s_hardware_baseline.smt_control, "on", sizeof(s_hardware_baseline.smt_control) - 1);
    }

    // 9. Bluetooth rfkill State (REF-REQ-063)
    s_hardware_baseline.bluetooth_blocked = false;
    for (int r = 0; r < 16; ++r) {
        char type_path[64];
        std::snprintf(type_path, sizeof(type_path), "/sys/class/rfkill/rfkill%d/type", r);
        n = 0;
        if (core::fs::read_small_file(type_path, buf, sizeof(buf) - 1, &n) && n > 0) {
            if (std::strncmp(buf, "bluetooth", 9) == 0) {
                char soft_path[64];
                std::snprintf(soft_path, sizeof(soft_path), "/sys/class/rfkill/rfkill%d/soft", r);
                n = 0;
                if (core::fs::read_small_file(soft_path, buf, sizeof(buf) - 1, &n) && n > 0) {
                    s_hardware_baseline.bluetooth_blocked = (buf[0] == '1');
                    break;
                }
            }
        }
    }

    // 10. Display Backlight Initial Brightness & Max Brightness (REF-REQ-063)
    const char* const bl_dirs[] = {
        "/sys/class/backlight/amdgpu_bl1",
        "/sys/class/backlight/amdgpu_bl0",
        "/sys/class/backlight/intel_backlight"
    };
    for (const char* bdir : bl_dirs) {
        char bpath[128];
        char mpath[128];
        std::snprintf(bpath, sizeof(bpath), "%s/brightness", bdir);
        std::snprintf(mpath, sizeof(mpath), "%s/max_brightness", bdir);
        n = 0;
        if (core::fs::read_small_file(mpath, buf, sizeof(buf) - 1, &n) && n > 0) {
            s_hardware_baseline.backlight_max = static_cast<uint32_t>(std::strtoul(buf, nullptr, 10));
            n = 0;
            if (core::fs::read_small_file(bpath, buf, sizeof(buf) - 1, &n) && n > 0) {
                s_hardware_baseline.backlight_brightness = static_cast<uint32_t>(std::strtoul(buf, nullptr, 10));
                break;
            }
        }
    }

    // 11. Kernel VM Writeback & Laptop Mode Baselines (REF-REQ-087, REF-ARCH-064)
    s_hardware_baseline.vm_dirty_writeback_centisecs = 500;
    s_hardware_baseline.vm_dirty_expire_centisecs = 3000;
    s_hardware_baseline.vm_laptop_mode = 0;
    s_hardware_baseline.vm_writeback_modified = false;

    n = 0;
    if (core::fs::read_small_file("/proc/sys/vm/dirty_writeback_centisecs", buf, sizeof(buf) - 1, &n) && n > 0) {
        buf[n] = '\0';
        s_hardware_baseline.vm_dirty_writeback_centisecs = static_cast<uint32_t>(std::strtoul(buf, nullptr, 10));
    }
    n = 0;
    if (core::fs::read_small_file("/proc/sys/vm/dirty_expire_centisecs", buf, sizeof(buf) - 1, &n) && n > 0) {
        buf[n] = '\0';
        s_hardware_baseline.vm_dirty_expire_centisecs = static_cast<uint32_t>(std::strtoul(buf, nullptr, 10));
    }
    n = 0;
    if (core::fs::read_small_file("/proc/sys/vm/laptop_mode", buf, sizeof(buf) - 1, &n) && n > 0) {
        buf[n] = '\0';
        s_hardware_baseline.vm_laptop_mode = static_cast<uint32_t>(std::strtoul(buf, nullptr, 10));
    }

    n = 0;
    char audio_buf[16];
    if (core::fs::read_small_file("/sys/module/snd_hda_intel/parameters/power_save", audio_buf, sizeof(audio_buf) - 1, &n) && n > 0) {
        audio_buf[n] = '\0';
        s_hardware_baseline.audio_power_save = std::atoi(audio_buf);
    }
    n = 0;
    if (core::fs::read_small_file("/sys/module/snd_hda_intel/parameters/power_save_controller", s_hardware_baseline.audio_power_save_controller, sizeof(s_hardware_baseline.audio_power_save_controller) - 1, &n) && n > 0) {
        s_hardware_baseline.audio_power_save_controller[n] = '\0';
        if (n > 0 && (s_hardware_baseline.audio_power_save_controller[n - 1] == '\n' || s_hardware_baseline.audio_power_save_controller[n - 1] == '\r')) {
            s_hardware_baseline.audio_power_save_controller[n - 1] = '\0';
        }
    }

    // 12. NVMe APST, Kernel CFS Migration Cost, GPU Power Profile & Wi-Fi Power
    //     Save Baselines (REF-REQ-092, REF-ARCH-069)
    n = 0;
    if (core::fs::read_small_file("/sys/module/nvme_core/parameters/default_ps_max_latency_us", buf, sizeof(buf) - 1, &n) && n > 0) {
        buf[n] = '\0';
        s_hardware_baseline.nvme_apst_latency_baseline_us = static_cast<uint32_t>(std::strtoul(buf, nullptr, 10));
    }
    n = 0;
    if (core::fs::read_small_file("/proc/sys/kernel/sched_migration_cost_ns", buf, sizeof(buf) - 1, &n) && n > 0) {
        buf[n] = '\0';
        s_hardware_baseline.sched_migration_cost_baseline_ns = std::strtoull(buf, nullptr, 10);
    }

    s_hardware_baseline.gpu_power_profile_mode_baseline = read_gpu_power_profile_mode();

    for (size_t i = 0; i < HardwareBaselineState::MAX_NVME_CONTROLLERS; ++i) {
        char ctrl_path[64];
        std::snprintf(ctrl_path, sizeof(ctrl_path), "/sys/class/nvme/nvme%zu/power/control", i);
        size_t cn = 0;
        if (!core::fs::read_small_file(ctrl_path, buf, sizeof(buf) - 1, &cn) || cn == 0) continue;
        while (cn > 0 && (buf[cn - 1] == '\n' || buf[cn - 1] == '\r' || buf[cn - 1] == ' ')) --cn;
        buf[cn] = '\0';
        std::strncpy(s_hardware_baseline.nvme_power_control_baseline[i], buf,
                     sizeof(s_hardware_baseline.nvme_power_control_baseline[i]) - 1);
    }

    char wifi_ifname[32];
    bool ps_enabled = true;
    if (detect_wireless_ifname(wifi_ifname, sizeof(wifi_ifname)) &&
        nl80211_get_power_save(wifi_ifname, &ps_enabled)) {
        s_hardware_baseline.wifi_power_save_baseline = ps_enabled;
    }

    // 13. ThinkPad fan level (REF-REQ-114) and SMU limits (REF-REQ-115)
    std::strncpy(s_hardware_baseline.fan_level_baseline, "auto",
                 sizeof(s_hardware_baseline.fan_level_baseline) - 1);
    {
        int rfd = ::open("/proc/acpi/ibm/fan", O_RDONLY | O_CLOEXEC);
        if (rfd >= 0) {
            char fbuf[256];
            ssize_t fn = ::read(rfd, fbuf, sizeof(fbuf) - 1);
            ::close(rfd);
            if (fn > 0) {
                fbuf[fn] = '\0';
                const char* lv = std::strstr(fbuf, "level:");
                if (lv != nullptr) {
                    lv += 6;
                    while (*lv == ' ' || *lv == '\t') ++lv;
                    size_t i = 0;
                    while (i < sizeof(s_hardware_baseline.fan_level_baseline) - 1 &&
                           lv[i] != '\0' && lv[i] != '\n' && lv[i] != '\r' && lv[i] != ' ') {
                        s_hardware_baseline.fan_level_baseline[i] = lv[i];
                        ++i;
                    }
                    s_hardware_baseline.fan_level_baseline[i] = '\0';
                    if (i == 0) std::strncpy(s_hardware_baseline.fan_level_baseline, "auto", 15);
                }
            }
        }
    }
    if (const char* tool = find_ryzenadj(); tool != nullptr) {
        char cmd[128];
        std::snprintf(cmd, sizeof(cmd), "%s -i 2>/dev/null", tool);
        FILE* p = ::popen(cmd, "r");
        if (p != nullptr) {
            char line[256];
            // ryzenadj -i prints pipe-separated rows. The first '|' followed by
            // the value; parse the leading number of the second column.
            auto parse_field = [](const char* l) -> double {
                const char* bar = std::strchr(l, '|');
                if (bar == nullptr) return 0.0;
                ++bar;
                while (*bar != '\0' && !((*bar >= '0' && *bar <= '9') || *bar == '-')) ++bar;
                return std::strtod(bar, nullptr);
            };
            auto parse_power_mw = [&](const char* l) -> uint32_t {
                const double w = parse_field(l);
                if (w <= 0.0) return 0;
                return static_cast<uint32_t>(w * 1000.0 + 0.5);
            };
            while (::fgets(line, sizeof(line), p) != nullptr) {
                // Power rows are in watts; the thermal row is already Celsius and
                // must NOT be scaled by 1000. Reading Tctl as mW would make the
                // restore write --tctl-temp=95000 instead of 95.
                if (std::strstr(line, "STAPM LIMIT") != nullptr) {
                    s_hardware_baseline.smu_stapm_mw = parse_power_mw(line);
                } else if (std::strstr(line, "PPT LIMIT FAST") != nullptr) {
                    s_hardware_baseline.smu_fast_mw = parse_power_mw(line);
                } else if (std::strstr(line, "PPT LIMIT SLOW") != nullptr) {
                    s_hardware_baseline.smu_slow_mw = parse_power_mw(line);
                } else if (std::strstr(line, "APU SLOW LIMIT") != nullptr) {
                    s_hardware_baseline.smu_apu_slow_mw = parse_power_mw(line);
                } else if (std::strstr(line, "THM LIMIT CORE") != nullptr) {
                    const double c = parse_field(line);
                    if (c > 0.0) s_hardware_baseline.smu_tctl_c = static_cast<uint32_t>(c + 0.5);
                }
            }
            ::pclose(p);
        }
    }

    s_hardware_baseline.captured = true;
}

void MitigationEngine::restore_hardware_baseline() noexcept {
    if (!s_hardware_baseline.captured) return;

    set_platform_profile(s_hardware_baseline.platform_profile);
    set_cpu_governor(s_hardware_baseline.cpu_governor);
    set_cpu_boost(s_hardware_baseline.cpu_boost != 0);
    set_pcie_aspm_policy(s_hardware_baseline.aspm_policy);
    if (s_hardware_baseline.scaling_max_freq_khz > 0) {
        set_cpu_scaling_max_freq(s_hardware_baseline.scaling_max_freq_khz);
    }
    set_panel_power_savings(s_hardware_baseline.panel_power_savings);
    restore_gpu_max_clock();
    if (s_hardware_baseline.gpu_dpm_level[0] != '\0') {
        set_gpu_dpm_level(s_hardware_baseline.gpu_dpm_level);
    }

    // Restore REF-REQ-063 UltraEndurance enhancements
    set_smt_control(s_hardware_baseline.smt_control);
    set_bluetooth_blocked(s_hardware_baseline.bluetooth_blocked);
    restore_display_backlight();
    if (s_hardware_baseline.drrs_applied) {
        set_display_refresh_rate(60);
    }
    if (s_hardware_baseline.kwin_blur_unloaded) {
        set_kwin_effects_suspended(false);
    }
    if (s_hardware_baseline.baloo_suspended) {
        set_baloo_suspended(false);
    }
    restore_wifi_txpower();
    if (s_hardware_baseline.vm_writeback_modified) {
        restore_vm_writeback_baseline();
    }
    if (s_hardware_baseline.audio_power_save_modified) {
        restore_audio_codec_baseline();
    }

    // Restore Ultimate Performance Unleash modifications (REF-REQ-092, REF-ARCH-069)
    release_performance_unleash();
    set_audio_latency_floor(false);
    if (s_hardware_baseline.audio_codec_power_save_suspended >= 0) {
        s_hardware_baseline.audio_codec_power_save_suspended = -1;
    }
}

const MitigationEngine::HardwareBaselineState& MitigationEngine::hardware_baseline() noexcept {
    return s_hardware_baseline;
}

const char* MitigationEngine::competing_power_manager() noexcept {
    // REF-REQ-109: /sys/firmware/acpi/platform_profile has exactly one correct
    // owner. power-profiles-daemon was found running on the development host,
    // set to "balanced", writing the same node WattCurb writes with a different
    // intention. Two writers with different intentions produce a nondeterministic
    // node, which is indistinguishable from a bug in either one.
    //
    // Scanned on demand rather than per cycle: this walks /proc, and the daemon's
    // evaluation loop runs every 3 s. Callers cache via s_competitor_checked.
    // The kernel truncates comm to 15 characters, so the process name and the
    // systemd unit are not the same string. Reporting the comm produced
    // "systemctl mask --now power-profiles-", which is not a unit and does not
    // work - the remedy in the alert has to be copy-pasteable.
    struct Competitor { const char* comm; const char* unit; };
    static const Competitor COMPETITORS[] = {
        { "power-profiles-", "power-profiles-daemon" },
        { "tuned",           "tuned"                },
        { "tlp",             "tlp"                  },
        { "auto-cpufreq",    "auto-cpufreq"         },
    };

    DIR* proc_dir = ::opendir("/proc");
    if (!proc_dir) return nullptr;

    const char* found = nullptr;
    struct dirent* entry = nullptr;
    while ((entry = ::readdir(proc_dir)) != nullptr && found == nullptr) {
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9') continue;

        char comm_path[64];
        std::snprintf(comm_path, sizeof(comm_path), "/proc/%s/comm", entry->d_name);
        char comm[32]{};
        size_t n = 0;
        if (!core::fs::read_small_file(comm_path, comm, sizeof(comm), &n) || n == 0) continue;
        while (n > 0 && (comm[n - 1] == '\n' || comm[n - 1] == '\r')) comm[--n] = '\0';

        for (const auto& c : COMPETITORS) {
            if (std::strcmp(comm, c.comm) == 0) { found = c.unit; break; }
        }
    }
    ::closedir(proc_dir);
    return found;
}

bool MitigationEngine::set_platform_profile(const char* profile) noexcept {
    if (!profile) return false;

    // Cheap idempotence: if the node already reads what we want, do nothing. This
    // makes the periodic re-assertion in the feature layer a read, not a fork.
    {
        char cur[32];
        size_t n = 0;
        if (core::fs::read_small_file("/sys/firmware/acpi/platform_profile", cur, sizeof(cur) - 1, &n) && n > 0) {
            cur[n] = '\0';
            while (n > 0 && (cur[n - 1] == '\n' || cur[n - 1] == '\r')) cur[--n] = '\0';
            if (std::strcmp(cur, profile) == 0) return true;
        }
    }

    static bool s_competitor_checked = false;
    static const char* s_competitor = nullptr;
    static uint32_t s_competitor_probe = 0;
    // Re-probe periodically, not once: a competitor (ppd) can be started after
    // this daemon, and a cached "no competitor" would then keep us writing the
    // node directly while ppd re-asserts its own value - an oscillation whose
    // loser is the user's clock. The probe walks /proc, so it is throttled.
    if (!s_competitor_checked || (++s_competitor_probe % 100u == 0u)) {
        s_competitor = competing_power_manager();
        s_competitor_checked = true;
    }

    if (s_competitor != nullptr) {
        // REF-REQ-109 originally declined the write entirely. On a host whose
        // power-profiles-daemon sat on "balanced" that left the EC power limit at
        // its balanced value, and under all-core load the CPU collapsed to
        // ~400 MHz while the UI still reported Performance - the mode's whole
        // promise is aggressive boost. Deferring is only safe if the competitor
        // is asked for the same profile; for ppd that is a supported request, and
        // for anything else we still decline rather than race an unknown writer.
        if (std::strstr(s_competitor, "power-profiles-daemon") != nullptr) {
            const char* ppd_name = "balanced";
            if (std::strcmp(profile, "performance") == 0) ppd_name = "performance";
            else if (std::strcmp(profile, "low-power") == 0) ppd_name = "power-saver";

            char cmd[96];
            std::snprintf(cmd, sizeof(cmd), "powerprofilesctl set %s 2>/dev/null", ppd_name);
            if (hw_system(cmd) == 0) return true;

            char detail[192];
            std::snprintf(detail, sizeof(detail),
                          "power-profiles-daemon did not accept platform_profile=%s (ppd '%s'); "
                          "the EC power limit may stay at its current value (REF-REQ-112.9)",
                          profile, ppd_name);
            core::EventLogger::log_alert("CONFLICT", detail);
            return false;
        }

        static bool s_logged = false;
        if (!s_logged) {
            s_logged = true;
            char detail[192];
            std::snprintf(detail, sizeof(detail),
                          "%s owns /sys/firmware/acpi/platform_profile; WattCurb will not write it. "
                          "Run 'systemctl mask --now %s' to give WattCurb full control.",
                          s_competitor, s_competitor);
            core::EventLogger::log_alert("CONFLICT", detail);
        }
        return false;
    }

    int fd = hw_open_write("/sys/firmware/acpi/platform_profile", O_WRONLY | O_CLOEXEC);
    if (fd < 0) return false;
    size_t len = std::strlen(profile);
    ssize_t w = ::write(fd, profile, len);
    (void)::write(fd, "\n", 1);
    ::close(fd);
    return (w > 0);
}

bool MitigationEngine::set_cpu_governor(const char* governor) noexcept {
    if (!governor) return false;
    size_t glen = std::strlen(governor);
    bool any_success = false;
    int total_cpus = get_total_online_cpus();

    for (int i = 0; i < total_cpus; ++i) {
        char path[128];
        std::snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", i);
        int fd = hw_open_write(path, O_WRONLY | O_CLOEXEC);
        if (fd >= 0) {
            if (::write(fd, governor, glen) > 0) {
                any_success = true;
            }
            ::close(fd);
        }
    }
    return any_success;
}

bool MitigationEngine::set_cpu_boost(bool enable) noexcept {
    int fd = hw_open_write("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY | O_CLOEXEC);
    if (fd < 0) return false;
    const char* val = enable ? "1\n" : "0\n";
    ssize_t w = ::write(fd, val, 2);
    ::close(fd);
    return (w > 0);
}

static uint64_t s_ceiling_assertion_count = 0;

uint64_t MitigationEngine::ceiling_assertion_count() noexcept {
    return s_ceiling_assertion_count;
}

bool MitigationEngine::is_frequency_starved(uint64_t observed_max_khz,
                                            uint64_t hw_max_khz,
                                            double load1,
                                            int32_t ncpu) noexcept {
    if (hw_max_khz == 0 || ncpu <= 0) return false;
    // Only when the machine is genuinely loaded: an idle box is supposed to sit
    // at its low clock, and tripping on that would fire the watchdog constantly.
    if (load1 < FREQ_STARVED_MIN_LOAD_RATIO * static_cast<double>(ncpu)) return false;
    return observed_max_khz <
           static_cast<uint64_t>(FREQ_STARVED_CLOCK_FRACTION * static_cast<double>(hw_max_khz));
}

bool MitigationEngine::assert_unrestricted_cpu_ceiling() noexcept {
    ++s_ceiling_assertion_count;
    // REF-REQ-112. Performance and Balanced promise an unrestricted CPU; this
    // is what enforces that promise, and it is deliberately idempotent so the
    // observation cycle can call it every tick. Each CPU is read before it is
    // written, so on a healthy machine the whole call is N pread()s and zero
    // writes - no sysfs write storm, no uncoordinated wakeup.
    uint32_t hw_max = s_hardware_baseline.hw_max_freq_khz;
    if (hw_max == 0) {
        char buf[32];
        size_t n = 0;
        if (core::fs::read_small_file("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq",
                                      buf, sizeof(buf) - 1, &n) && n > 0) {
            buf[n] = '\0';
            hw_max = static_cast<uint32_t>(std::strtoul(buf, nullptr, 10));
        }
        if (hw_max == 0) return false;
    }

    bool repaired = false;
    char freq_buf[32];
    const int flen = std::snprintf(freq_buf, sizeof(freq_buf), "%u\n", hw_max);
    const int total_cpus = get_total_online_cpus();

    for (int i = 0; i < total_cpus && i < 256; ++i) {
        char path[128];
        std::snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq", i);
        char cur[32];
        size_t cn = 0;
        if (core::fs::read_small_file(path, cur, sizeof(cur) - 1, &cn) && cn > 0) {
            cur[cn] = '\0';
            if (static_cast<uint32_t>(std::strtoul(cur, nullptr, 10)) >= hw_max) continue;
        }
        int fd = hw_open_write(path, O_WRONLY | O_CLOEXEC);
        if (fd < 0) continue;
        if (::write(fd, freq_buf, static_cast<size_t>(flen)) > 0) repaired = true;
        ::close(fd);
    }

    // The boost bit gates the P-state above the table's top entry. On acpi-cpufreq
    // (AMD CPB) cpuinfo_max_freq is the BASE clock, so a restored ceiling alone
    // still leaves the part pinned at base until this bit is set.
    //
    // REF-REQ-112.11: write it unconditionally. The sysfs value is not a reliable
    // mirror of the hardware state: this host was observed reading "1" while
    // cpupower reported "Active: no" and the single-core clock stayed at ~2.4 GHz,
    // and re-writing "1" restored 4.1 GHz. Gating the write on that read meant the
    // re-assertion skipped exactly the case it exists to repair. One small write
    // per observation cycle is the price of a reliable boost; `repaired` is left
    // alone so the per-cycle write does not turn into a REPAIR log storm.
    (void)set_cpu_boost(true);

    return repaired;
}

bool MitigationEngine::set_cpu_scaling_max_freq(uint32_t khz) noexcept {
    // REF-REQ-098: A ceiling below the driver's own minimum leaves the governor
    // with no valid operating point and the desktop crawling.
    {
        char buf[32];
        size_t n = 0;
        if (core::fs::read_small_file("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq",
                                      buf, sizeof(buf), &n) && n > 0) {
            buf[n] = '\0';
            const auto hw_min = static_cast<uint32_t>(std::strtoul(buf, nullptr, 10));
            if (hw_min > 0 && khz < hw_min) khz = hw_min;
        }
    }
    if (khz == 0) return false;

    // Detect hardware min freq to prevent kernel -EINVAL when requested freq is below hardware floor
    char min_buf[32];
    size_t mn = 0;
    uint32_t hw_min = 0;
    if (core::fs::read_small_file("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq", min_buf, sizeof(min_buf) - 1, &mn) && mn > 0) {
        min_buf[mn] = '\0';
        hw_min = static_cast<uint32_t>(std::strtoul(min_buf, nullptr, 10));
    }
    uint32_t target_khz = (hw_min > 0 && khz < hw_min) ? hw_min : khz;

    char freq_buf[32];
    int flen = std::snprintf(freq_buf, sizeof(freq_buf), "%u\n", target_khz);
    bool any_success = false;
    int total_cpus = get_total_online_cpus();

    for (int i = 0; i < total_cpus; ++i) {
        char path[128];
        std::snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq", i);
        int fd = hw_open_write(path, O_WRONLY | O_CLOEXEC);
        if (fd >= 0) {
            if (::write(fd, freq_buf, static_cast<size_t>(flen)) > 0) {
                any_success = true;
            }
            ::close(fd);
        }
    }
    return any_success;
}

bool MitigationEngine::set_panel_power_savings(uint32_t level) noexcept {
    char lvl_buf[16];
    int len = std::snprintf(lvl_buf, sizeof(lvl_buf), "%u\n", level);
    const char* paths[] = {
        "/sys/class/drm/card1-eDP-1/amdgpu/panel_power_savings",
        "/sys/class/drm/card0-eDP-1/amdgpu/panel_power_savings"
    };
    for (const char* p : paths) {
        int fd = hw_open_write(p, O_WRONLY | O_CLOEXEC);
        if (fd >= 0) {
            ssize_t w = ::write(fd, lvl_buf, static_cast<size_t>(len));
            ::close(fd);
            if (w > 0) return true;
        }
    }
    return false;
}

// REF-REQ-105: GPU clock control never touches the overdrive table.
//
// Both of these used to write pp_od_clk_voltage. The kernel only accepts that
// node while power_dpm_force_performance_level is "manual", and every profile
// application called restore_gpu_max_clock() with the level at auto/low/high.
// The kernel rejected each write:
//
//   amdgpu: pp_od_clk_voltage is not accessible if
//           power_dpm_force_performance_level is not in manual mode!
//
// 1706 rejected SMU transactions accumulated on the development machine and left
// the SMU unresponsive ("amdgpu: failed to write reg ..."), with the DPM table
// stuck reporting a 0 MHz active state. That destabilises DRM, and a Chromium
// GPU process cannot survive it - which is how an Electron application ended up
// hung with no way to recover short of a restart.
//
// The overdrive table is an overclocking interface: ASIC-specific, only valid in
// one mode, and capable of wedging the SMU when driven wrongly. Clock ceilings
// are now expressed solely through power_dpm_force_performance_level, which is
// mode-independent and is the supported mechanism. "low" already delivers the
// reduction UltraEndurance wanted from a 640 MHz overdrive cap.
bool MitigationEngine::set_gpu_max_clock(uint32_t mhz) noexcept {
    (void)mhz;
    return set_gpu_dpm_level("low");
}

bool MitigationEngine::restore_gpu_max_clock() noexcept {
    const char* baseline = s_hardware_baseline.gpu_dpm_level;
    return set_gpu_dpm_level((baseline[0] != '\0') ? baseline : "auto");
}

bool MitigationEngine::set_gpu_dpm_level(const char* level) noexcept {
    if (!level) return false;
    const char* const gpu_dirs[] = {
        "/sys/class/drm/card1/device",
        "/sys/class/drm/card0/device"
    };

    for (const char* dir : gpu_dirs) {
        char dpm_path[128];
        std::snprintf(dpm_path, sizeof(dpm_path), "%s/power_dpm_force_performance_level", dir);
        int fd = hw_open_write(dpm_path, O_WRONLY | O_CLOEXEC);
        if (fd >= 0) {
            (void)::write(fd, level, std::strlen(level));
            (void)::write(fd, "\n", 1);
            ::close(fd);
            return true;
        }
    }
    return false;
}

bool MitigationEngine::performance_pm_qos_held() noexcept {
    return s_hardware_baseline.performance_pm_qos_fd >= 0;
}

void MitigationEngine::set_performance_pm_qos(bool enable) noexcept {
    if (enable) {
        if (s_hardware_baseline.performance_pm_qos_fd < 0) {
            int fd = hw_open_write("/dev/cpu_dma_latency", O_RDWR | O_CLOEXEC);
            if (fd >= 0) {
                int32_t latency = 0; // 0 us target latency -> clamp to C0
                if (::write(fd, &latency, sizeof(latency)) == sizeof(latency)) {
                    s_hardware_baseline.performance_pm_qos_fd = fd;
                    s_hardware_baseline.performance_pm_qos_active = true;
                } else {
                    ::close(fd);
                }
            }
        }
    } else {
        if (s_hardware_baseline.performance_pm_qos_fd >= 0) {
            ::close(s_hardware_baseline.performance_pm_qos_fd);
            s_hardware_baseline.performance_pm_qos_fd = -1;
            s_hardware_baseline.performance_pm_qos_active = false;
        }
    }
}

bool MitigationEngine::set_gpu_power_profile_mode(int mode_id) noexcept {
    const char* const paths[] = {
        "/sys/class/drm/card1/device/pp_power_profile_mode",
        "/sys/class/drm/card0/device/pp_power_profile_mode"
    };
    for (const char* path : paths) {
        int fd = hw_open_write(path, O_WRONLY | O_CLOEXEC);
        if (fd >= 0) {
            char buf[16];
            int len = std::snprintf(buf, sizeof(buf), "%d\n", mode_id);
            (void)::write(fd, buf, static_cast<size_t>(len));
            ::close(fd);
            s_hardware_baseline.gpu_power_profile_mode_modified = true;
            return true;
        }
    }
    return false;
}

bool MitigationEngine::restore_gpu_power_profile_mode_baseline() noexcept {
    if (!s_hardware_baseline.gpu_power_profile_mode_modified) return true;
    // Fall back to 0 (BOOTUP_DEFAULT) only when the active row could not be read.
    const int baseline = s_hardware_baseline.gpu_power_profile_mode_baseline;
    set_gpu_power_profile_mode(baseline >= 0 ? baseline : 0);
    s_hardware_baseline.gpu_power_profile_mode_modified = false;
    return true;
}

bool MitigationEngine::set_nvme_apst_max_latency(uint32_t max_latency_us) noexcept {
    int fd = hw_open_write("/sys/module/nvme_core/parameters/default_ps_max_latency_us", O_WRONLY | O_CLOEXEC);
    if (fd >= 0) {
        char buf[32];
        int len = std::snprintf(buf, sizeof(buf), "%u\n", max_latency_us);
        (void)::write(fd, buf, static_cast<size_t>(len));
        ::close(fd);
        s_hardware_baseline.nvme_apst_modified = true;
    }
    return fd >= 0;
}

bool MitigationEngine::set_nvme_power_control(const char* value) noexcept {
    if (!value || value[0] == '\0') return false;

    bool actuated = false;
    for (size_t i = 0; i < HardwareBaselineState::MAX_NVME_CONTROLLERS; ++i) {
        char ctrl_path[64];
        std::snprintf(ctrl_path, sizeof(ctrl_path), "/sys/class/nvme/nvme%zu/power/control", i);
        int fd = hw_open_write(ctrl_path, O_WRONLY | O_CLOEXEC);
        if (fd < 0) continue;

        char out[16];
        int len = std::snprintf(out, sizeof(out), "%s\n", value);
        actuated = (::write(fd, out, static_cast<size_t>(len)) > 0) || actuated;
        ::close(fd);
    }
    if (actuated) s_hardware_baseline.nvme_power_control_modified = true;
    return actuated;
}

bool MitigationEngine::restore_nvme_power_control_baseline() noexcept {
    if (!s_hardware_baseline.nvme_power_control_modified) return true;

    for (size_t i = 0; i < HardwareBaselineState::MAX_NVME_CONTROLLERS; ++i) {
        const char* baseline = s_hardware_baseline.nvme_power_control_baseline[i];
        if (baseline[0] == '\0') continue; // controller absent or never captured

        char ctrl_path[64];
        std::snprintf(ctrl_path, sizeof(ctrl_path), "/sys/class/nvme/nvme%zu/power/control", i);
        int fd = hw_open_write(ctrl_path, O_WRONLY | O_CLOEXEC);
        if (fd < 0) continue;

        char out[16];
        int len = std::snprintf(out, sizeof(out), "%s\n", baseline);
        (void)::write(fd, out, static_cast<size_t>(len));
        ::close(fd);
    }
    s_hardware_baseline.nvme_power_control_modified = false;
    return true;
}

bool MitigationEngine::restore_nvme_apst_baseline() noexcept {
    restore_nvme_power_control_baseline();
    if (!s_hardware_baseline.nvme_apst_modified) return true;
    set_nvme_apst_max_latency(s_hardware_baseline.nvme_apst_latency_baseline_us);
    s_hardware_baseline.nvme_apst_modified = false;
    return true;
}

bool MitigationEngine::set_wifi_powersave(bool enable) noexcept {
    char ifname[32];
    if (!detect_wireless_ifname(ifname, sizeof(ifname))) {
        s_hardware_baseline.wifi_power_save_disabled = false;
        return false;
    }

    // (a) Bus-level runtime PM of the wireless device.
    char pm_path[96];
    std::snprintf(pm_path, sizeof(pm_path), "/sys/class/net/%s/device/power/control", ifname);
    bool pm_ok = false;
    int fd = hw_open_write(pm_path, O_WRONLY | O_CLOEXEC);
    if (fd >= 0) {
        const char* val = enable ? "auto\n" : "on\n";
        pm_ok = (::write(fd, val, std::strlen(val)) > 0);
        ::close(fd);
    }

    // (b) 802.11 power save via nl80211 generic netlink - no shell, no fork.
    const bool ps_ok = nl80211_set_power_save(ifname, enable);

    const bool actuated = pm_ok || ps_ok;
    s_hardware_baseline.wifi_power_save_disabled = actuated && !enable;
    return actuated;
}

bool MitigationEngine::restore_wifi_powersave_baseline() noexcept {
    if (!s_hardware_baseline.wifi_power_save_disabled) return true;
    set_wifi_powersave(s_hardware_baseline.wifi_power_save_baseline);
    // Cleared unconditionally: WattCurb no longer holds a power-save override,
    // even when the captured baseline was itself "power save disabled".
    s_hardware_baseline.wifi_power_save_disabled = false;
    return true;
}

void MitigationEngine::set_actuation_sandbox(bool enable) noexcept {
    s_actuation_sandbox = enable;
}

bool MitigationEngine::actuation_sandboxed() noexcept {
    return s_actuation_sandbox;
}

void MitigationEngine::release_performance_unleash() noexcept {
    set_performance_pm_qos(false);              // no-op when the fd was never acquired
    restore_gpu_power_profile_mode_baseline();
    restore_nvme_apst_baseline();
    restore_wifi_powersave_baseline();
    restore_sched_migration_cost_baseline();
    // REF-REQ-114/115: the raised SMU thermal limit and its thermal-assist fan
    // curve belong to Performance and Balanced only. Every transition out of
    // Performance (and the Balanced cleanup path below, which re-applies the SMU
    // raise) passes through here, so both are returned to the captured baseline
    // before a saving profile takes over. The next Performance/Balanced cycle
    // re-applies the fan curve from the temperature.
    restore_smu_limits();
    restore_fan_level();
    s_hardware_baseline.performance_unleash_engaged = false;
}

bool MitigationEngine::set_sched_migration_cost(uint64_t cost_ns) noexcept {
    // REF-REQ-107: this tunable does not exist on every scheduler. It is absent
    // on BORE/EEVDF kernels (verified missing on 7.2.5-1-cachyos), where this
    // actuator is a no-op. The false return is the caller's signal that the knob
    // was not applied; the *_modified flag below is only set on a real write, so
    // restore_hardware_baseline() will not write a value the daemon never set.
    int fd = hw_open_write("/proc/sys/kernel/sched_migration_cost_ns", O_WRONLY | O_CLOEXEC);
    if (fd >= 0) {
        char buf[32];
        int len = std::snprintf(buf, sizeof(buf), "%lu\n", cost_ns);
        (void)::write(fd, buf, static_cast<size_t>(len));
        ::close(fd);
        s_hardware_baseline.sched_migration_cost_modified = true;
        return true;
    }
    return false;
}

bool MitigationEngine::restore_sched_migration_cost_baseline() noexcept {
    if (!s_hardware_baseline.sched_migration_cost_modified) return true;
    set_sched_migration_cost(s_hardware_baseline.sched_migration_cost_baseline_ns);
    s_hardware_baseline.sched_migration_cost_modified = false;
    return true;
}

void MitigationEngine::enforce_cpu_freq_floor() noexcept {
    // REF-REQ-098: The scaling floor is pinned back to the driver's own minimum
    // on every profile application. WattCurb never lowers it, but the guarantee
    // the user needs is that the machine cannot be left crawling by anything -
    // a previous run, a firmware clamp, or another tool - so the floor is
    // asserted rather than merely not violated.
    char buf[32];
    size_t n = 0;
    if (!core::fs::read_small_file("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq",
                                   buf, sizeof(buf), &n) || n == 0) {
        return;
    }
    buf[n] = '\0';
    const auto hw_min = static_cast<uint32_t>(std::strtoul(buf, nullptr, 10));
    if (hw_min == 0) return;

    const int32_t cpus = get_total_online_cpus();
    for (int32_t i = 0; i < cpus && i < 256; ++i) {
        char path[96];
        std::snprintf(path, sizeof(path),
                      "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_min_freq", i);
        int fd = hw_open_write(path, O_WRONLY | O_CLOEXEC);
        if (fd < 0) continue;
        char val[24];
        const int len = std::snprintf(val, sizeof(val), "%u\n", hw_min);
        (void)::write(fd, val, static_cast<size_t>(len));
        ::close(fd);
    }
}

bool MitigationEngine::apply_power_profile(PowerProfileMode mode) noexcept {
    if (!s_hardware_baseline.captured) {
        capture_hardware_baseline();
    }

    // Asserted first, so no profile can leave the machine below its floor.
    enforce_cpu_freq_floor();

    switch (mode) {
    case PowerProfileMode::Performance:
        set_platform_profile("performance");
        set_cpu_governor("performance");
        set_cpu_boost(true);
        // REF-REQ-112: the hardware ceiling, not the captured value. See
        // HardwareBaselineState::hw_max_freq_khz for why the captured value is
        // not trustworthy as "what the user had".
        assert_unrestricted_cpu_ceiling();
        set_pcie_aspm_policy("performance");
        set_panel_power_savings(0);
        set_cpu_epp_policy("performance");
        restore_gpu_max_clock();
        // REF-REQ-092.9: leave the iGPU dynamic ("auto"), do NOT pin it to "high".
        // Measured on this Renoir APU: forcing the GPU to its highest level costs
        // the CPU ~28% of its all-core clock (1.42 GHz vs 1.97 GHz under an
        // 8-thread load), because the APU shares one power budget and a pinned-idle
        // iGPU spends it continuously. "auto" still lets the GPU boost to its
        // maximum on demand, so it is fully usable, without starving the CPU that
        // this profile exists to feed.
        set_gpu_dpm_level("auto");

        // Ultimate Performance Unleash Full-Silicon Actuations (REF-REQ-092, REF-ARCH-069)
        //
        // REF-REQ-107: the C0 clamp is NOT applied. Holding /dev/cpu_dma_latency
        // at 0 us was measured on this platform (Ryzen 7 PRO 4750U) to make a
        // fixed-work benchmark 1.88x SLOWER - 2.674 s against a 1.424 s baseline.
        // Zen's opportunistic boost is governed by accumulated power budget;
        // keeping every core in C0 spends that budget continuously instead of
        // letting idle cores return headroom, so the boost algorithm has less to
        // work with, not more. REQ-092.1 asserted the opposite without measuring
        // it. Any descriptor from a previous actuation is released here.
        set_performance_pm_qos(false);
        set_gpu_power_profile_mode(1);                // 3D_FULL_SCREEN peak compute/VRAM profile
        set_nvme_apst_max_latency(0);                 // Zero APST disk transition latency
        set_nvme_power_control("on");                 // Hold NVMe controllers runtime-active
        set_wifi_powersave(false);                    // Eliminate Wi-Fi power-save jitter
        set_sched_migration_cost(5000000);            // 5ms CPU cache warmth affinity
        // REF-REQ-115: raise the SMU thermal limit (85 C) and power ceilings so
        // the part can boost instead of throttling at the EC's conservative
        // default. The fan boost at 70 C (REF-REQ-114) keeps it below the limit.
        apply_smu_performance_limits();
        s_hardware_baseline.performance_unleash_engaged = true;

        // Restore UltraEndurance modifications if any
        set_smt_control(s_hardware_baseline.smt_control);
        set_bluetooth_blocked(s_hardware_baseline.bluetooth_blocked);
        restore_display_backlight();
        if (s_hardware_baseline.drrs_applied) set_display_refresh_rate(60);
        if (s_hardware_baseline.kwin_blur_unloaded) set_kwin_effects_suspended(false);
        if (s_hardware_baseline.baloo_suspended) set_baloo_suspended(false);
        restore_wifi_txpower();
        if (s_hardware_baseline.vm_writeback_modified) restore_vm_writeback_baseline();
        if (s_hardware_baseline.audio_power_save_modified) restore_audio_codec_baseline();
        return true;

    case PowerProfileMode::Balanced:
        // Demotion / Cleanup from Performance mode (REF-REQ-092)
        release_performance_unleash();

        set_platform_profile("balanced");
        set_cpu_governor("schedutil");
        set_cpu_boost(true);
        assert_unrestricted_cpu_ceiling(); // REF-REQ-112
        // REF-REQ-115: Balanced also raises the SMU thermal limit to 85 C so it
        // throttles on temperature rather than on the EC's conservative default.
        apply_smu_performance_limits();
        set_pcie_aspm_policy(s_hardware_baseline.aspm_policy);
        set_panel_power_savings(1);
        set_cpu_epp_policy("balance_performance");
        restore_gpu_max_clock();
        set_gpu_dpm_level("auto");
        // Restore UltraEndurance modifications if any
        set_smt_control(s_hardware_baseline.smt_control);
        set_bluetooth_blocked(s_hardware_baseline.bluetooth_blocked);
        restore_display_backlight();
        if (s_hardware_baseline.drrs_applied) set_display_refresh_rate(60);
        if (s_hardware_baseline.kwin_blur_unloaded) set_kwin_effects_suspended(false);
        if (s_hardware_baseline.baloo_suspended) set_baloo_suspended(false);
        restore_wifi_txpower();
        if (s_hardware_baseline.vm_writeback_modified) restore_vm_writeback_baseline();
        if (s_hardware_baseline.audio_power_save_modified) restore_audio_codec_baseline();
        return true;

    case PowerProfileMode::PowerSaver:
        // Demotion / Cleanup from Performance mode (REF-REQ-092)
        release_performance_unleash();

        set_platform_profile("low-power");
        set_cpu_governor("schedutil");
        set_cpu_boost(false);
        set_cpu_scaling_max_freq(1700000); // 1.7GHz base clock cap
        set_pcie_aspm_policy("powersave");
        set_panel_power_savings(2);
        set_cpu_epp_policy("balance_power");
        restore_gpu_max_clock();
        set_gpu_dpm_level("auto");
        // Restore UltraEndurance modifications if any
        set_smt_control(s_hardware_baseline.smt_control);
        set_bluetooth_blocked(s_hardware_baseline.bluetooth_blocked);
        restore_display_backlight();
        if (s_hardware_baseline.drrs_applied) set_display_refresh_rate(60);
        if (s_hardware_baseline.kwin_blur_unloaded) set_kwin_effects_suspended(false);
        if (s_hardware_baseline.baloo_suspended) set_baloo_suspended(false);
        restore_wifi_txpower();
        if (s_hardware_baseline.vm_writeback_modified) restore_vm_writeback_baseline();
        if (s_hardware_baseline.audio_power_save_modified) restore_audio_codec_baseline();
        return true;

    case PowerProfileMode::UltraEndurance:
        // Demotion / Cleanup from Performance mode (REF-REQ-092)
        release_performance_unleash();

        set_platform_profile("low-power");
        set_cpu_governor("powersave");
        set_cpu_boost(false);
        set_cpu_scaling_max_freq(1400000); // 1.4GHz minimum hardware P-state floor
        if (!set_pcie_aspm_policy("powersupersave")) {
            set_pcie_aspm_policy("powersave");
        }
        set_panel_power_savings(2);
        set_cpu_epp_policy("power");
        set_gpu_max_clock(640); // 40% GPU clock cap (640MHz of 1600MHz)
        // REF-REQ-088 Dimension 1: GPU DPM low & 3-Tier VRAM GC
        set_gpu_dpm_level("low");
        trigger_3tier_vram_gc();
        // REF-REQ-063, REF-REQ-064, REF-REQ-065: Ultra-low power hardware & desktop extensions
        // REF-REQ-098: SMT is deliberately NOT disabled. Offlining half the logical
        // CPUs in the mode whose failure mode is stalling removes exactly the
        // scheduling capacity the compositor and input path need, and CPU hotplug
        // against the engine's own affinity masks can strand a task on a core that
        // is going away. Deep saving comes from clocks and idle residency here,
        // not from taking processors out of the scheduler.
        set_bluetooth_blocked(false); // REF-REQ-065: Bluetooth Always-On Invariant
        cap_display_backlight(35.0);
        set_display_refresh_rate(48); // REF-REQ-063: 48Hz DRRS on user Ultra Save
        set_kwin_effects_suspended(true);
        set_baloo_suspended(true);
        set_wifi_txpower_limit(1200); // REF-REQ-064: Cap Wi-Fi Tx to 12.00 dBm (16mW RF)
        // REF-REQ-087 & REF-ARCH-064 Dimension 3: Kernel VM Writeback & Laptop Mode Coalescing
        // REF-REQ-108: writeback coalescing is bounded so the flush cannot become a
        // stall. A 60 s window lets a whole minute of dirty pages accumulate and
        // discharge in one burst, which blocks every fsync behind it and widens the
        // data-loss window on power cut to the same 60 s. The original 60 s/120 s
        // pairing was chosen to let a spinning disk stay parked; the only block
        // device on this class of machine is NVMe, which has no spin-up to amortise,
        // so the long window buys little and costs responsiveness.
        set_vm_dirty_writeback_centisecs(ULTRA_DIRTY_WRITEBACK_CS); // 15 seconds
        set_vm_dirty_expire_centisecs(ULTRA_DIRTY_EXPIRE_CS);       // 30 seconds
        set_vm_laptop_mode(2);                  // Batch flushing, bounded
        s_hardware_baseline.vm_writeback_modified = true;
        // REF-REQ-088 Dimension 4: PCIe & USB Runtime PM auto
        apply_pcie_runtime_pm_auto();
        apply_usb_runtime_pm_auto();
        s_hardware_baseline.pcie_runtime_pm_modified = true;
        s_hardware_baseline.usb_runtime_pm_modified = true;
        // REF-REQ-088 Dimension 5: Audio Codec Autosuspend
        set_audio_codec_power_save(10, true);
        return true;
    }
    return false;
}

PowerProfileMode MitigationEngine::resolve_profile(
    PowerProfileMode current, bool on_battery, double battery_pct,
    ProfileDemotionLatch& latch
) noexcept {
    // Rearm once the battery recovers past each threshold. The +5% guard band
    // stops a reading hovering on the boundary from re-triggering.
    if (!on_battery || battery_pct > 35.0) latch.crossed_30 = false;
    if (!on_battery || battery_pct > 25.0) latch.crossed_20 = false;

    // On AC the daemon never imposes a profile - the user's choice stands.
    if (!on_battery) return current;

    // Critical floor. Enforced continuously rather than latched: below 5% no
    // other profile is permitted at all.
    if (battery_pct <= 5.0) return PowerProfileMode::UltraEndurance;

    // The latch records the THRESHOLD CROSSING, not the demotion. Otherwise a
    // user who picks Performance again at 25% would be demoted a second time.
    // A threshold may only ever move TOWARDS more saving. The enum is ordered
    // Performance < Balanced < PowerSaver < UltraEndurance, so a user sitting in
    // UltraEndurance is never pulled back up to PowerSaver by the 20% rule.
    if (battery_pct <= 20.0 && !latch.crossed_20) {
        latch.crossed_20 = true;
        latch.crossed_30 = true; // one large drop must not demote twice
        if (current < PowerProfileMode::PowerSaver) {
            return PowerProfileMode::PowerSaver;
        }
    }

    if (battery_pct <= 30.0 && !latch.crossed_30) {
        latch.crossed_30 = true;
        if (current == PowerProfileMode::Performance) {
            return PowerProfileMode::Balanced;
        }
    }

    return current;
}

PowerProfileMode MitigationEngine::determine_profile(bool on_battery, double battery_pct) noexcept {
    // The user's explicit selection, when present, is the baseline the battery
    // rules act on - not something they silently override every cycle.
    const PowerProfileMode current = m_profile_override.value_or(m_current_profile);
    const PowerProfileMode target = resolve_profile(current, on_battery, battery_pct, m_demotion_latch);

    // A demotion becomes the new baseline, so it is not undone on the next tick.
    if (m_profile_override.has_value() && target != *m_profile_override) {
        m_profile_override = target;
    }
    return target;
}

bool MitigationEngine::resolve_cgroup_path(int32_t pid, char* out_buf, size_t out_cap) noexcept {
    if (pid <= 1 || !out_buf || out_cap < 32) return false;

    char proc_path[64];
    std::snprintf(proc_path, sizeof(proc_path), "/proc/%d/cgroup", pid);

    int fd = ::open(proc_path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;

    char read_buf[512];
    ssize_t bytes_read = ::read(fd, read_buf, sizeof(read_buf) - 1);
    ::close(fd);

    if (bytes_read <= 0) return false;
    read_buf[bytes_read] = '\0';

    // cgroup v2 format: "0::<path>\n"
    const char* ptr = read_buf;
    const char* end = read_buf + bytes_read;

    while (ptr < end) {
        if (ptr[0] == '0' && ptr[1] == ':' && ptr[2] == ':') {
            const char* path_start = ptr + 3;
            const char* line_end = path_start;
            while (line_end < end && *line_end != '\n' && *line_end != '\r') {
                ++line_end;
            }

            size_t path_len = static_cast<size_t>(line_end - path_start);
            // Construct full sysfs path: /sys/fs/cgroup + path
            constexpr const char base[] = "/sys/fs/cgroup";
            constexpr size_t base_len = sizeof(base) - 1;

            if (base_len + path_len + 1 >= out_cap) {
                return false; // Truncation guard
            }

            std::memcpy(out_buf, base, base_len);
            if (path_len > 0 && path_start[0] == '/') {
                std::memcpy(out_buf + base_len, path_start, path_len);
                out_buf[base_len + path_len] = '\0';
            } else if (path_len > 0) {
                out_buf[base_len] = '/';
                std::memcpy(out_buf + base_len + 1, path_start, path_len);
                out_buf[base_len + 1 + path_len] = '\0';
            } else {
                out_buf[base_len] = '\0';
            }
            return true;
        }

        // Advance to next line
        while (ptr < end && *ptr != '\n') {
            ++ptr;
        }
        if (ptr < end && *ptr == '\n') {
            ++ptr;
        }
    }

    return false;
}

bool MitigationEngine::is_immune_process(int32_t pid) noexcept {
    WATTCURB_PROFILE_SCOPE("mitig.is_immune");
    if (pid <= 1) return true;

    char comm_path[64];
    std::snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", pid);
    char comm_buf[64]{};
    size_t n = 0;
    if (!core::fs::read_small_file(comm_path, comm_buf, sizeof(comm_buf), &n) || n == 0) {
        return false;
    }

    while (n > 0 && (comm_buf[n - 1] == '\n' || comm_buf[n - 1] == '\r' || comm_buf[n - 1] == ' ')) {
        comm_buf[--n] = '\0';
    }
    std::string_view comm(comm_buf, n);

    // REF-REQ-096: Whatever currently owns a running PCM stream is immune by
    // identity, not by name - a sound server the allowlist has never heard of
    // still must not be throttled while it is moving samples.
    if (is_audio_owner(pid)) return true;

    // Implements REF-REQ-049 & REF-REQ-054: Absolute immunity invariant for audio and critical system daemons
    if (comm.starts_with("pipewire") || comm.starts_with("wireplumber") ||
        comm == "pulseaudio" || comm.starts_with("jackd") || comm == "jackdbus" ||
        comm == "alsactl" || comm == "rtkit-daemon" || comm == "sndiod" ||
        comm == "systemd" || comm == "init" || comm == "kthreadd" ||
        comm.starts_with("kworker") || comm == "dbus-broker" || comm == "dbus-daemon" ||
        comm == "seatd" || comm == "polkitd" || comm == "udevd" ||
        comm == "kwin_wayland" || comm == "kwin_x11" || comm == "kwin" ||
        comm == "mutter" || comm == "sway" || comm == "hyprland" ||
        comm == "Xorg" || comm == "Xwayland" || comm.starts_with("wattcurb")) {
        return true;
    }
    return false;
}

int32_t MitigationEngine::get_total_online_cpus() noexcept {
    static const int32_t s_cpus = []() noexcept {
        long n = ::sysconf(_SC_NPROCESSORS_ONLN);
        return (n > 0) ? static_cast<int32_t>(n) : 1;
    }();
    return s_cpus;
}

int32_t MitigationEngine::get_reserved_headroom_cores() noexcept {
    static const int32_t s_reserved = []() noexcept {
        int32_t n = get_total_online_cpus();
        if (n >= 8) return 2; // Reserve 1 physical SMT core pair (e.g. Cores 14 & 15)
        if (n >= 4) return 1; // Reserve 1 logical core
        return 0;             // Systems with < 4 cores cannot reserve without major penalty
    }();
    return s_reserved;
}

cpu_set_t MitigationEngine::get_headroom_allowed_cpuset(PowerProfileMode mode) noexcept {
    int32_t n = get_total_online_cpus();
    int32_t allowed = n;

    if (n >= 8) {
        switch (mode) {
            case PowerProfileMode::UltraEndurance:
                // Ultra Mode: Balanced 50% max cores (e.g. 8 cores on 16-core, 4 cores on 8-core)
                // to prevent starvation and excess power drain while operating at 1.4GHz floor
                allowed = std::max(2, n / 2);
                break;
            case PowerProfileMode::PowerSaver:
                // PowerSaver Mode: 75% max cores (e.g. 12 cores on 16-core)
                allowed = std::max(4, (n * 3) / 4);
                break;
            case PowerProfileMode::Balanced:
            case PowerProfileMode::Performance:
                // Balanced & Performance: Reserve 2 clean headroom cores (e.g. 14 cores on 16-core)
                allowed = std::max(1, n - 2);
                break;
        }
    } else if (n >= 4) {
        switch (mode) {
            case PowerProfileMode::UltraEndurance:
                allowed = std::max(1, n / 2); // 2 cores on 4-core (50% max CPU)
                break;
            case PowerProfileMode::PowerSaver:
            case PowerProfileMode::Balanced:
            case PowerProfileMode::Performance:
                allowed = std::max(1, n - 1);
                break;
        }
    }

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int32_t c = 0; c < allowed; ++c) {
        CPU_SET(static_cast<size_t>(c), &cpuset);
    }
    return cpuset;
}

cpu_set_t MitigationEngine::get_all_cores_cpuset() noexcept {
    static const cpu_set_t s_all = []() noexcept {
        int32_t n = get_total_online_cpus();
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        for (int32_t c = 0; c < n; ++c) {
            CPU_SET(static_cast<size_t>(c), &cpuset);
        }
        return cpuset;
    }();
    return s_all;
}

namespace {

void parse_cpulist(const char* buf, cpu_set_t& set) noexcept {
    CPU_ZERO(&set);
    if (!buf) return;
    const char* p = buf;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == ',') ++p;
        if (!*p) break;
        char* end = nullptr;
        long first = std::strtol(p, &end, 10);
        if (end == p) break;
        p = end;
        long last = first;
        if (*p == '-') {
            ++p;
            last = std::strtol(p, &end, 10);
            if (end != p) p = end;
        }
        if (first >= 0 && last >= first && last < CPU_SETSIZE) {
            for (long c = first; c <= last; ++c) {
                CPU_SET(static_cast<size_t>(c), &set);
            }
        }
    }
}

bool is_interactive_terminal_or_shell(std::string_view comm) noexcept {
    return (comm == "konsole" || comm == "alacritty" || comm == "kitty" ||
            comm == "foot" || comm == "wezterm" || comm == "ptyxis" ||
            comm == "gnome-terminal" || comm == "xterm" || comm == "rxvt" ||
            comm == "bash" || comm == "zsh" || comm == "fish" ||
            comm == "tmux" || comm == "screen" || comm == "ssh");
}

bool is_desktop_compositor(std::string_view comm) noexcept {
    return (comm == "kwin_wayland" || comm == "kwin_x11" || comm == "kwin" ||
            comm == "mutter" || comm == "sway" || comm == "hyprland" ||
            comm == "Xorg" || comm == "Xwayland" || comm == "weston");
}

} // namespace

const MitigationEngine::CpuClusterTopology& MitigationEngine::get_cluster_topology() noexcept {
    static const CpuClusterTopology s_topo = []() noexcept {
        CpuClusterTopology topo{};
        topo.total_cpus = get_total_online_cpus();
        topo.all_cores_cpuset = get_all_cores_cpuset();
        topo.cluster_count = 1;

        // Auto-detect physical L3 cache cluster topology via sysfs (REF-RES-020, REF-REQ-084, REF-ARCH-061)
        bool detected_l3 = false;
        int fd0 = ::open("/sys/devices/system/cpu/cpu0/cache/index3/shared_cpu_list", O_RDONLY | O_CLOEXEC);
        if (fd0 >= 0) {
            char buf[64]{0};
            ssize_t n0 = ::read(fd0, buf, sizeof(buf) - 1);
            ::close(fd0);
            if (n0 > 0) {
                buf[n0] = '\0';
                parse_cpulist(buf, topo.c1_cpuset);

                // Find first CPU not in c1_cpuset to discover Cluster 2
                int32_t second_cpu = -1;
                for (int32_t c = 0; c < topo.total_cpus; ++c) {
                    if (!CPU_ISSET(static_cast<size_t>(c), &topo.c1_cpuset)) {
                        second_cpu = c;
                        break;
                    }
                }

                if (second_cpu >= 0) {
                    char path[96];
                    std::snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cache/index3/shared_cpu_list", second_cpu);
                    int fd1 = ::open(path, O_RDONLY | O_CLOEXEC);
                    if (fd1 >= 0) {
                        char buf2[64]{0};
                        ssize_t n1 = ::read(fd1, buf2, sizeof(buf2) - 1);
                        ::close(fd1);
                        if (n1 > 0) {
                            buf2[n1] = '\0';
                            parse_cpulist(buf2, topo.c2_cpuset);
                            topo.cluster_count = 2;
                            detected_l3 = true;
                        }
                    }
                }
            }
        }

        // Fallback for uniform L3 or single-cluster CPUs
        if (!detected_l3 || topo.cluster_count < 2) {
            CPU_ZERO(&topo.c1_cpuset);
            CPU_ZERO(&topo.c2_cpuset);
            if (topo.total_cpus >= 4) {
                int32_t half = topo.total_cpus / 2;
                for (int32_t c = 0; c < half; ++c) {
                    CPU_SET(static_cast<size_t>(c), &topo.c1_cpuset);
                }
                for (int32_t c = half; c < topo.total_cpus; ++c) {
                    CPU_SET(static_cast<size_t>(c), &topo.c2_cpuset);
                }
                topo.cluster_count = 2;
            } else {
                topo.c1_cpuset = topo.all_cores_cpuset;
                topo.c2_cpuset = topo.all_cores_cpuset;
                topo.cluster_count = 1;
            }
        }

        // Interactive Headroom within C1 (e.g. Cores 0..3 on 8-core C1)
        CPU_ZERO(&topo.interactive_shield_cpuset);
        int32_t c1_size = 0;
        for (int32_t c = 0; c < topo.total_cpus; ++c) {
            if (CPU_ISSET(static_cast<size_t>(c), &topo.c1_cpuset)) ++c1_size;
        }
        int32_t shield_cores = std::max(1, c1_size / 2);
        int32_t counted = 0;
        for (int32_t c = 0; c < topo.total_cpus && counted < shield_cores; ++c) {
            if (CPU_ISSET(static_cast<size_t>(c), &topo.c1_cpuset)) {
                CPU_SET(static_cast<size_t>(c), &topo.interactive_shield_cpuset);
                ++counted;
            }
        }

        return topo;
    }();
    return s_topo;
}

cpu_set_t MitigationEngine::get_c1_cpuset() noexcept {
    return get_cluster_topology().c1_cpuset;
}

cpu_set_t MitigationEngine::get_c2_cpuset() noexcept {
    return get_cluster_topology().c2_cpuset;
}

cpu_set_t MitigationEngine::get_interactive_shield_cpuset() noexcept {
    return get_cluster_topology().interactive_shield_cpuset;
}

bool MitigationEngine::shield_interactive_process(int32_t pid, std::string_view comm) noexcept {
    if (pid <= 1) return false;

    bool is_comp = is_desktop_compositor(comm);
    int target_nice = is_comp ? -10 : -5;

    int cur_nice = ::getpriority(PRIO_PROCESS, static_cast<id_t>(pid));
    if (cur_nice > target_nice) {
        hw_setpriority(PRIO_PROCESS, static_cast<id_t>(pid), target_nice);
    }

    // REF-REQ-099: Cluster pinning is a CACHE-LOCALITY optimisation, but it is
    // also a hard capacity cut - it hands the process half the machine. In
    // Performance mode that is exactly backwards: the foreground application must
    // never be given less than the whole processor, and a parallel build or a
    // browser pinned to one CCX stalls visibly. Priority elevation above is kept,
    // because that ADDS resources; the affinity restriction is not applied.
    if (s_effective_profile == PowerProfileMode::Performance) {
        cpu_set_t all_cores = get_all_cores_cpuset();
        hw_sched_setaffinity(pid, sizeof(cpu_set_t), &all_cores);
        return true;
    }

    // Saving profiles keep the single-CCX residency, which lets the other cluster
    // stay in a deeper idle state.
    const auto& topo = get_cluster_topology();
    hw_sched_setaffinity(pid, sizeof(cpu_set_t), &topo.c1_cpuset);
    return true;
}

void MitigationEngine::shield_all_interactive_terminals() noexcept {
    WATTCURB_PROFILE_SCOPE("mitig.shield_terminals");
    DIR* proc_dir = ::opendir("/proc");
    if (!proc_dir) return;

    struct dirent* entry = nullptr;
    while ((entry = ::readdir(proc_dir)) != nullptr) {
        if (entry->d_type != DT_DIR && entry->d_type != DT_UNKNOWN) continue;
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9') continue;

        int32_t pid = std::atoi(entry->d_name);
        if (pid <= 1) continue;

        char comm_path[64];
        std::snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", pid);
        int fd = ::open(comm_path, O_RDONLY | O_CLOEXEC);
        if (fd < 0) continue;

        char comm_buf[64];
        ssize_t n = ::read(fd, comm_buf, sizeof(comm_buf) - 1);
        ::close(fd);
        if (n <= 0) continue;
        if (comm_buf[n - 1] == '\n') --n;
        comm_buf[n] = '\0';
        std::string_view comm(comm_buf, static_cast<size_t>(n));

        if (is_interactive_terminal_or_shell(comm) || is_desktop_compositor(comm)) {
            shield_interactive_process(pid, comm);
        }
    }
    ::closedir(proc_dir);
}

bool MitigationEngine::is_heavy_compute_candidate(const ProcessAttributedPower& proc) noexcept {
    if (proc.pid <= 1) return false;
    auto tier = static_cast<ProcessSafetyTier>(proc.safety_tier);
    if (tier == ProcessSafetyTier::CriticalImmune || tier == ProcessSafetyTier::DesktopCore) {
        return false;
    }

    std::string_view comm(proc.comm.c_str());
    if (comm.starts_with("wattcurb") || comm.starts_with("pipewire") || comm.starts_with("wireplumber")) {
        return false;
    }
    bool is_compiler_or_builder = 
        (comm == "gcc" || comm == "g++" || comm == "clang" || comm == "clang++" ||
         comm == "rustc" || comm == "cargo" || comm == "ninja" || comm == "make" ||
         comm == "python3" || comm == "python" || comm == "node" || comm == "java" ||
         comm == "ffmpeg" || comm == "as" || comm == "ld" || comm.starts_with("cc1") ||
         comm.starts_with("rust-lld") || comm.starts_with("Isolated Web Co"));

    if (is_compiler_or_builder && (proc.cpu_watts > 0.4 || proc.num_threads >= 2)) {
        return true;
    }
    if (proc.cpu_watts > 0.8) {
        return true;
    }
    if (proc.num_threads >= 4 && proc.cpu_watts > 0.25) {
        return true;
    }
    if (proc.wdi_score > 4.0 && proc.cpu_watts > 0.20) {
        return true;
    }
    return false;
}


bool MitigationEngine::apply_core_affinity_cap(int32_t pid, const cpu_set_t* allowed_set) noexcept {
    WATTCURB_PROFILE_SCOPE("mitig.apply_affinity_cap");
    if (pid <= 1 || is_immune_process(pid)) return false;

    cpu_set_t default_set;
    if (!allowed_set) {
        default_set = get_headroom_allowed_cpuset();
        allowed_set = &default_set;
    }

    bool any_success = (hw_sched_setaffinity(pid, sizeof(cpu_set_t), allowed_set) == 0);

    // Thread-level traversal via /proc/<pid>/task/ (REF-REQ-054, REF-ARCH-030)
    char task_dir[64];
    std::snprintf(task_dir, sizeof(task_dir), "/proc/%d/task", pid);

    DIR* dir = ::opendir(task_dir);
    if (!dir) {
        return any_success;
    }

    struct dirent* entry = nullptr;
    while ((entry = ::readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        char* endptr = nullptr;
        long tid = std::strtol(entry->d_name, &endptr, 10);
        if (tid > 0 && *endptr == '\0') {
            if (hw_sched_setaffinity(static_cast<pid_t>(tid), sizeof(cpu_set_t), allowed_set) == 0) {
                any_success = true;
            }
        }
    }
    ::closedir(dir);
    return any_success;
}

bool MitigationEngine::restore_core_affinity(int32_t pid, const cpu_set_t* target_affinity) noexcept {
    WATTCURB_PROFILE_SCOPE("mitig.restore_affinity");
    if (pid <= 1) return false;

    cpu_set_t all_cores;
    const cpu_set_t* mask_to_set = target_affinity;
    if (!mask_to_set) {
        all_cores = get_all_cores_cpuset();
        mask_to_set = &all_cores;
    }

    bool any_success = (hw_sched_setaffinity(pid, sizeof(cpu_set_t), mask_to_set) == 0);

    char task_dir[64];
    std::snprintf(task_dir, sizeof(task_dir), "/proc/%d/task", pid);

    DIR* dir = ::opendir(task_dir);
    if (!dir) {
        return any_success;
    }

    struct dirent* entry = nullptr;
    while ((entry = ::readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        char* endptr = nullptr;
        long tid = std::strtol(entry->d_name, &endptr, 10);
        if (tid > 0 && *endptr == '\0') {
            if (hw_sched_setaffinity(static_cast<pid_t>(tid), sizeof(cpu_set_t), mask_to_set) == 0) {
                any_success = true;
            }
        }
    }
    ::closedir(dir);
    return any_success;
}

bool MitigationEngine::apply_sched_batch(int32_t pid, int nice_val) noexcept {
    WATTCURB_PROFILE_SCOPE("mitig.apply_sched_batch");
    if (pid <= 1 || is_immune_process(pid)) return false;

    struct sched_param sp{};
    sp.sched_priority = 0;
    bool any_success = (hw_sched_setscheduler(pid, SCHED_BATCH, &sp) == 0);
    hw_setpriority(PRIO_PROCESS, static_cast<id_t>(pid), nice_val);

    char task_dir[64];
    std::snprintf(task_dir, sizeof(task_dir), "/proc/%d/task", pid);

    DIR* dir = ::opendir(task_dir);
    if (!dir) {
        return any_success;
    }

    struct dirent* entry = nullptr;
    while ((entry = ::readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        char* endptr = nullptr;
        long tid = std::strtol(entry->d_name, &endptr, 10);
        if (tid > 0 && *endptr == '\0') {
            if (hw_sched_setscheduler(static_cast<pid_t>(tid), SCHED_BATCH, &sp) == 0) {
                any_success = true;
            }
            hw_setpriority(PRIO_PROCESS, static_cast<id_t>(tid), nice_val);
        }
    }
    ::closedir(dir);
    return any_success;
}


void MitigationEngine::refresh_audio_stream_state() noexcept {
    WATTCURB_PROFILE_SCOPE("mitig.audio_probe");
    AudioStreamState st{};

    // ALSA publishes both facts we need in one small file per substream:
    //   state: RUNNING      -> a stream is actually moving samples
    //   owner_pid   : <pid> -> the process holding the PCM (the sound server)
    DIR* snd = ::opendir("/proc/asound");
    if (!snd) {
        s_audio_state = st;
        return;
    }

    struct dirent* card = nullptr;
    while ((card = ::readdir(snd)) != nullptr) {
        if (std::strncmp(card->d_name, "card", 4) != 0) continue;
        if (card->d_name[4] < '0' || card->d_name[4] > '9') continue;

        char card_path[96];
        if (std::snprintf(card_path, sizeof(card_path), "/proc/asound/%s", card->d_name)
                >= static_cast<int>(sizeof(card_path))) {
            continue;
        }

        DIR* cd = ::opendir(card_path);
        if (!cd) continue;

        struct dirent* pcm = nullptr;
        while ((pcm = ::readdir(cd)) != nullptr) {
            const size_t len = std::strlen(pcm->d_name);
            // Playback substreams only: "pcm<N>p".
            if (len < 5 || std::strncmp(pcm->d_name, "pcm", 3) != 0 || pcm->d_name[len - 1] != 'p') {
                continue;
            }

            for (int sub_idx = 0; sub_idx < 4; ++sub_idx) {
                char status_path[192];
                if (std::snprintf(status_path, sizeof(status_path), "%s/%s/sub%d/status",
                                  card_path, pcm->d_name, sub_idx)
                        >= static_cast<int>(sizeof(status_path))) {
                    break;
                }

                char buf[256];
                size_t n = 0;
                if (!core::fs::read_small_file(status_path, buf, sizeof(buf), &n) || n == 0) break;
                buf[n] = '\0';

                if (std::strstr(buf, "RUNNING") == nullptr) continue;
                st.active = true;

                const char* owner = std::strstr(buf, "owner_pid");
                if (!owner) continue;
                const char* colon = std::strchr(owner, ':');
                if (!colon) continue;

                const long pid_val = std::strtol(colon + 1, nullptr, 10);
                if (pid_val <= 1) continue;

                const auto pid = static_cast<int32_t>(pid_val);
                bool dup = false;
                for (size_t i = 0; i < st.owner_count; ++i) {
                    if (st.owner_pids[i] == pid) { dup = true; break; }
                }
                if (!dup && st.owner_count < MAX_AUDIO_OWNERS) {
                    st.owner_pids[st.owner_count++] = pid;
                }
            }
        }
        ::closedir(cd);
    }
    ::closedir(snd);

    s_audio_state = st;
}

const MitigationEngine::AudioStreamState& MitigationEngine::audio_stream_state() noexcept {
    return s_audio_state;
}

bool MitigationEngine::is_audio_owner(int32_t pid) noexcept {
    if (pid <= 1) return false;
    for (size_t i = 0; i < s_audio_state.owner_count; ++i) {
        if (s_audio_state.owner_pids[i] == pid) return true;
    }
    return false;
}

void MitigationEngine::set_effective_profile(PowerProfileMode mode) noexcept {
    s_effective_profile = mode;
}

PowerProfileMode MitigationEngine::effective_profile() noexcept {
    return s_effective_profile;
}

bool MitigationEngine::restore_process_affinity(int32_t pid, const cpu_set_t& original) noexcept {
    if (pid <= 1) return false;

    // A zeroed record would pin the process to nothing at all, so fall back to
    // the full online set rather than stranding it.
    if (CPU_COUNT(&original) == 0) {
        cpu_set_t all_cores = get_all_cores_cpuset();
        return hw_sched_setaffinity(pid, sizeof(cpu_set_t), &all_cores) == 0;
    }
    return hw_sched_setaffinity(pid, sizeof(cpu_set_t), &original) == 0;
}

bool MitigationEngine::is_graphical_session_process(int32_t pid) noexcept {
    if (pid <= 1) return false;

    // The daemon runs as root, so it can read any environ. A graphical client
    // carries the session's display variables; a system service does not.
    char env_path[64];
    std::snprintf(env_path, sizeof(env_path), "/proc/%d/environ", pid);

    int fd = ::open(env_path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;

    char buf[4096];
    const ssize_t n = ::read(fd, buf, sizeof(buf) - 1);
    ::close(fd);
    if (n <= 0) return false;

    // environ is NUL-separated; scan entry by entry.
    const size_t len = static_cast<size_t>(n);
    buf[len] = '\0';
    for (size_t i = 0; i < len; ) {
        const char* entry = buf + i;
        const size_t elen = std::strlen(entry);
        if (std::strncmp(entry, "WAYLAND_DISPLAY=", 16) == 0 ||
            std::strncmp(entry, "DISPLAY=", 8) == 0) {
            return true;
        }
        i += elen + 1;
        if (elen == 0) break;
    }
    return false;
}

bool MitigationEngine::is_liveness_critical(int32_t pid) noexcept {
    if (pid <= 1) return true;

    char comm_path[64];
    std::snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", pid);
    char comm_buf[64]{};
    size_t n = 0;
    if (!core::fs::read_small_file(comm_path, comm_buf, sizeof(comm_buf), &n) || n == 0) {
        return false;
    }
    while (n > 0 && (comm_buf[n - 1] == '\n' || comm_buf[n - 1] == '\r' || comm_buf[n - 1] == ' ')) {
        comm_buf[--n] = '\0';
    }

    const auto cls = ProcessClassifierDB::classify(std::string_view(comm_buf, n));
    return cls.tier <= ProcessSafetyTier::DesktopShell;
}

bool MitigationEngine::is_audio_shielded(int32_t pid) noexcept {
    if (!s_audio_state.active || pid <= 1) return false;

    char comm_path[64];
    std::snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", pid);
    char comm_buf[64]{};
    size_t n = 0;
    if (!core::fs::read_small_file(comm_path, comm_buf, sizeof(comm_buf), &n) || n == 0) {
        return false;
    }
    while (n > 0 && (comm_buf[n - 1] == '\n' || comm_buf[n - 1] == '\r' || comm_buf[n - 1] == ' ')) {
        comm_buf[--n] = '\0';
    }

    const auto cls = ProcessClassifierDB::classify(std::string_view(comm_buf, n));
    // Tier 0..3 (CriticalImmune .. UserInteractive) are shielded while audio runs;
    // Tier 4/5 background workers remain throttleable.
    return cls.tier <= ProcessSafetyTier::UserInteractive;
}

bool MitigationEngine::is_stall_shielded(int32_t pid) noexcept {
    // REF-REQ-108: the audio-playback shield (is_audio_shielded) protects Tier 0..3
    // only while a PCM stream is RUNNING. Outside playback the same processes were
    // demotable to SCHED_IDLE with 100 ms timer slack, which is what a stall looks
    // like from the keyboard. UltraEndurance is allowed to be SLOW - that is the
    // contract of a 1.4 GHz ceiling - but it is not allowed to stop responding.
    //
    // This shield is profile-independent and playback-independent: Tier 0..3 are
    // never moved to the idle class in any profile. Tier 4/5 background workers
    // (baloo, updatedb, runaway scripts) remain fully throttleable, so the saving
    // is not abandoned - it is confined to work nobody is waiting on.
    if (pid <= 1) return false;

    char comm_path[64];
    std::snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", pid);
    char comm_buf[64]{};
    size_t n = 0;
    if (!core::fs::read_small_file(comm_path, comm_buf, sizeof(comm_buf), &n) || n == 0) {
        return false;
    }
    while (n > 0 && (comm_buf[n - 1] == '\n' || comm_buf[n - 1] == '\r' || comm_buf[n - 1] == ' ')) {
        comm_buf[--n] = '\0';
    }

    const auto cls = ProcessClassifierDB::classify(std::string_view(comm_buf, n));
    return cls.tier <= ProcessSafetyTier::UserInteractive;
}

void MitigationEngine::set_audio_latency_floor(bool engage) noexcept {
    if (engage) {
        if (s_hardware_baseline.audio_pm_qos_fd < 0) {
            int fd = hw_open_write("/dev/cpu_dma_latency", O_RDWR | O_CLOEXEC);
            if (fd >= 0) {
                int32_t latency = AUDIO_DMA_LATENCY_US;
                if (::write(fd, &latency, sizeof(latency)) == sizeof(latency)) {
                    s_hardware_baseline.audio_pm_qos_fd = fd;
                } else {
                    ::close(fd);
                }
            }
        }
    } else if (s_hardware_baseline.audio_pm_qos_fd >= 0) {
        ::close(s_hardware_baseline.audio_pm_qos_fd);
        s_hardware_baseline.audio_pm_qos_fd = -1;
    }
}

void MitigationEngine::heal_over_throttled_processes() noexcept {
    WATTCURB_PROFILE_SCOPE("mitig.heal_over_throttled");

    DIR* proc_dir = ::opendir("/proc");
    if (!proc_dir) return;

    cpu_set_t all_cores = get_all_cores_cpuset();
    struct dirent* entry = nullptr;
    while ((entry = ::readdir(proc_dir)) != nullptr) {
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9') continue;

        const int32_t pid = std::atoi(entry->d_name);
        if (pid <= 1) continue;

        // Cheap gate first: only processes actually sitting in the idle class are
        // candidates, so the protective checks run on a handful of PIDs.
        if (::sched_getscheduler(pid) != SCHED_IDLE) continue;

        const bool protect = is_immune_process(pid)
                          || is_liveness_critical(pid)
                          || is_audio_shielded(pid)
                          || is_graphical_session_process(pid);
        if (!protect) continue;

        // restore_sched_normal() walks /proc/<pid>/task, so every thread comes
        // back - restoring only the thread group leader leaves the process just
        // as unresponsive, which is what made the first manual recovery attempt
        // look like it had failed.
        restore_sched_normal(pid);
        apply_timer_slack(pid, 50'000ULL);
        hw_sched_setaffinity(pid, sizeof(cpu_set_t), &all_cores);
    }
    ::closedir(proc_dir);
}

namespace {

inline bool cpuset_equal(const cpu_set_t& a, const cpu_set_t& b) noexcept {
    return CPU_EQUAL(&a, &b);
}

} // namespace

bool MitigationEngine::mask_matches_engine_pattern(const cpu_set_t& mask) noexcept {
    // Every mask this engine is capable of applying to another process.
    const auto& topo = get_cluster_topology();
    if (cpuset_equal(mask, topo.c1_cpuset)) return true;
    if (cpuset_equal(mask, topo.c2_cpuset)) return true;
    if (cpuset_equal(mask, topo.interactive_shield_cpuset)) return true;

    for (const auto m : { PowerProfileMode::Performance, PowerProfileMode::Balanced,
                          PowerProfileMode::PowerSaver, PowerProfileMode::UltraEndurance }) {
        const cpu_set_t headroom = get_headroom_allowed_cpuset(m);
        if (cpuset_equal(mask, headroom)) return true;
    }

    // SMT-sibling-excluding variants: one logical CPU per physical core within a
    // cluster. ksecretd was found pinned to {8,10,12,14} on this host - every
    // other CPU of the C2 cluster (8-15) - and no function in the engine today
    // produces that set. It is the residue of an earlier dispersion algorithm
    // that has since been rewritten.
    //
    // A mask can outlive the code that applied it by an arbitrary number of
    // releases, so matching only what the CURRENT engine emits leaves damage
    // permanently unrepairable. These stride-2 variants are added from direct
    // observation, not speculation; a pattern nobody has seen is not added here.
    for (const cpu_set_t* base : { &topo.c1_cpuset, &topo.c2_cpuset, &topo.all_cores_cpuset }) {
        cpu_set_t strided;
        CPU_ZERO(&strided);
        int32_t taken = 0;
        for (int32_t c = 0; c < CPU_SETSIZE && c < 1024; ++c) {
            if (!CPU_ISSET(static_cast<size_t>(c), base)) continue;
            if ((taken++ % 2) == 0) CPU_SET(static_cast<size_t>(c), &strided);
        }
        if (CPU_COUNT(&strided) > 0 && cpuset_equal(mask, strided)) return true;
    }

    return false;
}

void MitigationEngine::repair_orphaned_affinity_masks() noexcept {
    WATTCURB_PROFILE_SCOPE("mitig.repair_affinity");

    // REF-REQ-110: An affinity mask is PROCESS state. Stopping the daemon
    // restores hardware knobs and nothing else, so a mask applied by a previous
    // run outlives the daemon, outlives a restart, and is inherited by every
    // child - a shell masked to half the machine hands that half to everything
    // launched from it, for the rest of the session.
    //
    // This was found on the development host: plasmashell, ksecretd (pinned to
    // the strided set 8,10,12,14), several KDE services and two login shells were
    // all confined to 8 of 16 logical CPUs with no daemon running.
    //
    // heal_over_throttled_processes() cannot catch this: it gates on
    // SCHED_IDLE for cost reasons, and a masked process sits in a normal
    // scheduling class. The repair therefore runs once at bootstrap.
    //
    // Scope is deliberately narrow. A mask WattCurb would never have applied -
    // one on a process it does not shield - may be a deliberate taskset by the
    // user, and is left alone.
    const int32_t online = get_total_online_cpus();
    if (online <= 1) return;

    cpu_set_t all_cores = get_all_cores_cpuset();

    DIR* proc_dir = ::opendir("/proc");
    if (!proc_dir) return;

    int32_t repaired = 0;
    struct dirent* entry = nullptr;
    while ((entry = ::readdir(proc_dir)) != nullptr) {
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9') continue;

        const int32_t pid = std::atoi(entry->d_name);
        if (pid <= 1) continue;

        cpu_set_t mask;
        CPU_ZERO(&mask);
        if (::sched_getaffinity(pid, sizeof(cpu_set_t), &mask) != 0) continue;
        if (CPU_COUNT(&mask) >= online) continue; // already has the whole machine

        // REF-REQ-110.2 (revised): match the MASK, not the process.
        //
        // The first version scoped the repair by process class - liveness
        // critical, graphical session, stall shielded, immune. On the host that
        // motivated this it repaired 66 processes but missed ksecretd (pinned to
        // 8,10,12,14) and the agent's own shell, because neither classifies into
        // those buckets. The classifier's opinion of a process has nothing to do
        // with whether WattCurb masked it.
        //
        // A mask that is exactly one of the masks this engine can produce is
        // WattCurb's work. A deliberate `taskset -c 3` by the user is not one of
        // them and is still left alone, which is what the scope limit was for.
        if (!mask_matches_engine_pattern(mask)) continue;

        if (hw_sched_setaffinity(pid, sizeof(cpu_set_t), &all_cores) == 0) {
            ++repaired;
        }
    }
    ::closedir(proc_dir);

    if (repaired > 0) {
        char detail[128];
        std::snprintf(detail, sizeof(detail),
                      "Restored full CPU affinity to %d shielded process(es) left masked by a previous run",
                      repaired);
        core::EventLogger::log_alert("REPAIR", detail);
    }
}

void MitigationEngine::audit_and_heal_audio_stack() noexcept {
    WATTCURB_PROFILE_SCOPE("mitig.audit_heal_audio");

    // REF-REQ-103: Release anything protected that is sitting in the idle class,
    // whatever applied it and whether or not it was ever tracked.
    heal_over_throttled_processes();

    // REF-REQ-096: Audio continuity applies in EVERY profile, not just Performance.
    refresh_audio_stream_state();
    set_audio_latency_floor(s_audio_state.active);

    if (s_audio_state.active) {
        // HDA codec runtime suspend must not arm underneath a live stream: the
        // resume costs a codec power-up and is audible. Park it at 0 and record
        // what to put back once playback stops.
        if (s_hardware_baseline.audio_codec_power_save_suspended < 0) {
            char ps_buf[16];
            size_t ps_n = 0;
            int cur_ps = 0;
            if (core::fs::read_small_file("/sys/module/snd_hda_intel/parameters/power_save",
                                          ps_buf, sizeof(ps_buf), &ps_n) && ps_n > 0) {
                ps_buf[ps_n] = '\0';
                cur_ps = std::atoi(ps_buf);
            }
            if (cur_ps > 0) {
                s_hardware_baseline.audio_codec_power_save_suspended = cur_ps;
                set_audio_codec_power_save(0, false);
            }
        }
    } else if (s_hardware_baseline.audio_codec_power_save_suspended >= 0) {
        set_audio_codec_power_save(s_hardware_baseline.audio_codec_power_save_suspended, true);
        s_hardware_baseline.audio_codec_power_save_suspended = -1;
    }
    // Implements REF-REQ-049 & REF-REQ-054: Dynamic discovery of user session slices and active audio healing
    const char* services[] = {
        "pipewire.service",
        "pipewire-pulse.service",
        "wireplumber.service",
        "pulseaudio.service",
        "plasma-kwin_wayland.service",
        "app-kwin_wayland.service"
    };
    const char* slices[] = {"session.slice", "app.slice"};

    cpu_set_t all_cores = get_all_cores_cpuset();

    // 1. Discover active user slices under /sys/fs/cgroup/user.slice/
    DIR* user_dir = ::opendir("/sys/fs/cgroup/user.slice");
    if (user_dir) {
        struct dirent* uent = nullptr;
        while ((uent = ::readdir(user_dir)) != nullptr) {
            if (std::strncmp(uent->d_name, "user-", 5) != 0) continue;
            const char* dot = std::strstr(uent->d_name, ".slice");
            if (!dot || dot[6] != '\0') continue;

            // Extract uid string from user-<uid>.slice
            char uid_str[32]{};
            size_t uid_len = static_cast<size_t>(dot - (uent->d_name + 5));
            if (uid_len >= sizeof(uid_str)) continue;
            std::memcpy(uid_str, uent->d_name + 5, uid_len);
            uid_str[uid_len] = '\0';

            for (const char* svc : services) {
                for (const char* slc : slices) {
                    char path[384];
                    std::snprintf(path, sizeof(path),
                                  "/sys/fs/cgroup/user.slice/%s/user@%s.service/%s/%s/cgroup.procs",
                                  uent->d_name, uid_str, slc, svc);

                    char buf[128]{};
                    size_t n = 0;
                    if (!core::fs::read_small_file(path, buf, sizeof(buf), &n) || n == 0) continue;

                    bool is_comp = (std::strstr(svc, "kwin") != nullptr);
                    int target_nice = is_comp ? -10 : -19;

                    const char* p = buf;
                    while (*p) {
                        while (*p == ' ' || *p == '\n' || *p == '\r') ++p;
                        if (!*p) break;
                        char* next = nullptr;
                        long pid_val = std::strtol(p, &next, 10);
                        if (pid_val > 1) {
                            int32_t audio_pid = static_cast<int32_t>(pid_val);
                            int sched = ::sched_getscheduler(audio_pid);
                            if (sched == SCHED_IDLE) {
                                restore_sched_normal(audio_pid);
                            }
                            // Elevate priority to real-time interactive level (-19 for audio, -10 for compositor)
                            hw_setpriority(PRIO_PROCESS, static_cast<id_t>(audio_pid), target_nice);
                            // Ensure audio/compositor threads have full access to all cores including clean headroom
                            hw_sched_setaffinity(audio_pid, sizeof(cpu_set_t), &all_cores);
                        }
                        if (next == p) break;
                        p = next;
                    }
                }
            }
        }
        ::closedir(user_dir);
    }
}

bool MitigationEngine::apply_sched_idle(int32_t pid) noexcept {
    if (pid <= 1) return false;

    // REF-REQ-049: Never throttle audio or critical system processes under any circumstance
    if (is_immune_process(pid)) return false;

    // REF-REQ-096.6: Nothing that may be feeding the audio pipeline gets demoted
    // to SCHED_IDLE while a stream is running.
    if (is_audio_shielded(pid)) return false;

    // REF-REQ-108: and Tier 0..3 are shielded whether or not audio is playing.
    if (is_stall_shielded(pid)) return false;

    // REF-REQ-098: Input and window management keep a working share in every
    // profile - a compositor that cannot be scheduled looks like a hung machine.
    if (is_liveness_critical(pid)) return false;

    // REF-REQ-101: An application in the user's graphical session is never
    // demoted to the idle class. The classifier falls back to Tier 5 for any name
    // it has not been taught, which meant an app the user was working in - one
    // desktop chat client, in the reported case - was pinned to a quarter of the
    // cores at SCHED_IDLE and stopped responding. Absence from a hardcoded list
    // is not evidence of being a runaway.
    if (is_graphical_session_process(pid)) return false;

    // 1. Set CPU scheduler to SCHED_IDLE
    struct sched_param sp{};
    sp.sched_priority = 0;
    int sched_ret = hw_sched_setscheduler(pid, SCHED_IDLE, &sp);

    // 2. Set Block I/O scheduler to IOPRIO_CLASS_IDLE
#ifdef SYS_ioprio_set
    int prio_val = IOPRIO_PRIO_VALUE(IOPRIO_CLASS_IDLE, 7);
    int io_ret = static_cast<int>(hw_ioprio_set(IOPRIO_WHO_PROCESS, pid, prio_val));
    return (sched_ret == 0 || io_ret == 0);
#else
    return (sched_ret == 0);
#endif
}

bool MitigationEngine::restore_sched_normal(int32_t pid, int original_policy, int original_nice) noexcept {
    if (pid <= 1) return false;

    int target_policy = (original_policy >= 0) ? original_policy : SCHED_OTHER;

    // 1. Reset nice priority to original_nice
    hw_setpriority(PRIO_PROCESS, static_cast<id_t>(pid), original_nice);

    // 2. Restore CPU scheduler to original_policy
    struct sched_param sp{};
    sp.sched_priority = 0;
    int sched_ret = hw_sched_setscheduler(pid, target_policy, &sp);

    // Thread-level traversal for all tasks
    char task_dir[64];
    std::snprintf(task_dir, sizeof(task_dir), "/proc/%d/task", pid);
    DIR* dir = ::opendir(task_dir);
    if (dir) {
        struct dirent* entry = nullptr;
        while ((entry = ::readdir(dir)) != nullptr) {
            if (entry->d_name[0] == '.') continue;
            char* endptr = nullptr;
            long tid = std::strtol(entry->d_name, &endptr, 10);
            if (tid > 0 && *endptr == '\0') {
                hw_sched_setscheduler(static_cast<pid_t>(tid), target_policy, &sp);
                hw_setpriority(PRIO_PROCESS, static_cast<id_t>(tid), original_nice);
            }
        }
        ::closedir(dir);
    }

    // 3. Restore Block I/O scheduler to Best-Effort (IOPRIO_CLASS_BE, priority 4)
#ifdef SYS_ioprio_set
    int prio_val = IOPRIO_PRIO_VALUE(IOPRIO_CLASS_BE, 4);
    int io_ret = static_cast<int>(hw_ioprio_set(IOPRIO_WHO_PROCESS, pid, prio_val));
    return (sched_ret == 0 || io_ret == 0);
#else
    return (sched_ret == 0);
#endif
}

bool MitigationEngine::apply_timer_slack(int32_t pid, uint64_t slack_ns) noexcept {
    if (pid <= 1 || slack_ns == 0) return false;

    // REF-REQ-101: Relaxing timer slack had NO immunity check at all - not even
    // is_immune_process - so any process could be given a 100 ms slack. For an
    // event-loop application that is indistinguishable from being asleep: every
    // timer, animation tick and IPC timeout can land 100 ms late. A desktop chat
    // client was left in exactly this state and reported as frozen.
    //
    // Lowering slack back towards the default is always allowed; only RELAXING it
    // is gated, so restore paths keep working.
    constexpr uint64_t DEFAULT_TIMERSLACK_NS = 50'000ULL;
    if (slack_ns > DEFAULT_TIMERSLACK_NS) {
        if (is_immune_process(pid)) return false;
        if (is_liveness_critical(pid)) return false;
        if (is_audio_shielded(pid)) return false;
        if (is_graphical_session_process(pid)) return false;
    }

    char proc_path[64];
    std::snprintf(proc_path, sizeof(proc_path), "/proc/%d/timerslack_ns", pid);

    int fd = hw_open_write(proc_path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) return false;

    char num_buf[32];
    int len = std::snprintf(num_buf, sizeof(num_buf), "%lu\n", static_cast<unsigned long>(slack_ns));
    ssize_t written = ::write(fd, num_buf, static_cast<size_t>(len));
    ::close(fd);

    return (written > 0);
}

bool MitigationEngine::apply_memory_reclaim(int32_t pid, uint64_t bytes, bool file_only) noexcept {
    if (pid <= 1 || bytes == 0) return false;

    char cg_path[256];
    if (!resolve_cgroup_path(pid, cg_path, sizeof(cg_path))) {
        return false;
    }

    char reclaim_path[320];
    std::snprintf(reclaim_path, sizeof(reclaim_path), "%s/memory.reclaim", cg_path);

    int fd = hw_open_write(reclaim_path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) return false;

    // REF-REQ-112: "swappiness=0" restricts the reclaim to file-backed pages.
    // Without it the kernel is free to satisfy the request by writing anonymous
    // pages to swap, which is the opposite of what the caller wants when swap
    // is the resource running out.
    char num_buf[48];
    int len = file_only
                  ? std::snprintf(num_buf, sizeof(num_buf), "%lu swappiness=0\n", static_cast<unsigned long>(bytes))
                  : std::snprintf(num_buf, sizeof(num_buf), "%lu\n", static_cast<unsigned long>(bytes));
    ssize_t written = ::write(fd, num_buf, static_cast<size_t>(len));

    // Kernels before 6.4 reject the swappiness argument outright. Fall back to
    // a plain reclaim rather than losing the tier entirely.
    if (written < 0 && file_only) {
        len = std::snprintf(num_buf, sizeof(num_buf), "%lu\n", static_cast<unsigned long>(bytes));
        written = ::write(fd, num_buf, static_cast<size_t>(len));
    }
    ::close(fd);

    return (written > 0);
}

bool MitigationEngine::apply_cgroup_freeze(int32_t pid, bool freeze) noexcept {
    // REF-REQ-096.6: Freezing a media-pipeline process mid-playback guarantees a
    // dropout, so it is refused outright while a stream is running.
    if (freeze && is_audio_shielded(pid)) return false;

    // REF-REQ-108: Tier 0..3 are shielded whether or not audio is playing.
    if (freeze && is_stall_shielded(pid)) return false;

    // REF-REQ-098: Freezing the compositor or the desktop shell hangs the session.
    if (freeze && is_liveness_critical(pid)) return false;

    // REF-REQ-101: Nor may a graphical-session application be frozen.
    if (freeze && is_graphical_session_process(pid)) return false;
    if (pid <= 1) return false;

    // REF-REQ-044 & REF-RES-015: Absolute Zero-Kill & Zero-Freeze Invariant
    // Freezing user/system processes (cgroup.freeze = 1) causes D-Bus IPC deadlocks,
    // tree-wide application freezes, and process termination. WattCurb strictly forbids
    // freezing processes under any circumstance!
    if (freeze) {
        // Fallback safely to non-halting graceful idle throttling
        apply_sched_idle(pid);
        apply_timer_slack(pid, 100'000'000ULL);
        return false; // Prohibit cgroup freeze
    }

    // Thawing (unfreezing) is safely executed to recover any previously frozen process
    char cg_path[256];
    if (!resolve_cgroup_path(pid, cg_path, sizeof(cg_path))) {
        return false;
    }

    char freeze_path[320];
    std::snprintf(freeze_path, sizeof(freeze_path), "%s/cgroup.freeze", cg_path);

    int fd = hw_open_write(freeze_path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) return false;

    ssize_t written = ::write(fd, "0\n", 2);
    ::close(fd);

    return (written > 0);
}

bool MitigationEngine::apply_cgroup_cpu_quota(int32_t pid, uint32_t max_quota_us, uint32_t period_us) noexcept {
    if (pid <= 1 || is_immune_process(pid)) return false;

    char cg_path[256];
    if (!resolve_cgroup_path(pid, cg_path, sizeof(cg_path))) {
        return false;
    }

    char cpu_max_path[320];
    std::snprintf(cpu_max_path, sizeof(cpu_max_path), "%s/cpu.max", cg_path);

    int fd = hw_open_write(cpu_max_path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) return false;

    char buf[64];
    int len = std::snprintf(buf, sizeof(buf), "%u %u\n", max_quota_us, period_us);
    ssize_t written = ::write(fd, buf, static_cast<size_t>(len));
    ::close(fd);

    return (written > 0);
}

bool MitigationEngine::restore_cgroup_cpu_quota(int32_t pid) noexcept {
    if (pid <= 1) return false;

    char cg_path[256];
    if (!resolve_cgroup_path(pid, cg_path, sizeof(cg_path))) {
        return false;
    }

    char cpu_max_path[320];
    std::snprintf(cpu_max_path, sizeof(cpu_max_path), "%s/cpu.max", cg_path);

    int fd = hw_open_write(cpu_max_path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) return false;

    constexpr const char unconstrained[] = "max 100000\n";
    ssize_t written = ::write(fd, unconstrained, sizeof(unconstrained) - 1);
    ::close(fd);

    return (written > 0);
}

bool MitigationEngine::set_pcie_aspm_policy(const char* policy) noexcept {
    if (!policy) return false;
    int fd = hw_open_write("/sys/module/pcie_aspm/parameters/policy", O_WRONLY | O_CLOEXEC);
    if (fd < 0) return false;

    size_t len = std::strlen(policy);
    ssize_t written = ::write(fd, policy, len);
    ::close(fd);
    return (written > 0);
}

bool MitigationEngine::set_cpu_epp_policy(const char* policy) noexcept {
    if (!policy) return false;
    // The EPP knob lives under the cpufreq policy directory. The previous path
    // (/sys/devices/system/cpu/cpu0/power/energy_performance_preference) does
    // not exist on any real driver, so this actuator was a permanent no-op while
    // the UI still reported "CPU EPP: ...". Write every online CPU's policy and
    // report the real outcome so the summary cannot claim a change that did not
    // happen.
    const int32_t cpus = get_total_online_cpus();
    bool any = false;
    for (int32_t c = 0; c < cpus; ++c) {
        char path[128];
        std::snprintf(path, sizeof(path),
                      "/sys/devices/system/cpu/cpu%d/cpufreq/energy_performance_preference", c);
        int fd = hw_open_write(path, O_WRONLY | O_CLOEXEC);
        if (fd < 0) continue;
        ssize_t w = ::write(fd, policy, std::strlen(policy));
        ::close(fd);
        if (w > 0) any = true;
    }
    s_epp_applied = any;
    return any;
}

bool MitigationEngine::set_smt_control(const char* state) noexcept {
    if (!state) return false;
    int fd = hw_open_write("/sys/devices/system/cpu/smt/control", O_WRONLY | O_CLOEXEC);
    if (fd < 0) return false;
    ssize_t w = ::write(fd, state, std::strlen(state));
    (void)::write(fd, "\n", 1);
    ::close(fd);
    return (w > 0);
}

// ---------------------------------------------------------------------------
// REF-REQ-114: ThinkPad thermal-assist fan curve (Performance / Balanced)
// ---------------------------------------------------------------------------
namespace {
// Last level the curve actually wrote. A per-cycle write would be a needless
// sysfs storm on a 1-3 s observation loop, so the curve only writes on a
// change. It is reset by restore_fan_level(), otherwise returning to the
// captured baseline and then re-entering Performance at the same temperature
// would be mistaken for "no change" and leave the EC in control.
int g_last_fan_level = -1;
// REF-TEST-073: counts evaluations from the production entry point so a wiring
// that only runs under test is falsifiable, exactly like ceiling_assertion_count.
uint64_t g_fan_curve_application_count = 0;
} // namespace

int MitigationEngine::fan_level_for_temp(double cpu_temp_c) noexcept {
    if (cpu_temp_c <= 0.0) return -1; // no temperature reading
    // REF-REQ-114 (defect fix): the top step is written as the NUMERIC level 7,
    // never as the string "full-speed". On this host (T14/P14s class, Renoir)
    // `level full-speed` is accepted by thinkpad_acpi but resolves to
    // `level: disengaged` - the EC takes the fan back. The write still returns
    // success, so the old code cached level 7 and then never retried at the same
    // temperature, pinning the fan to the EC's quiet curve (~4.3k RPM) while the
    // daemon believed it had pinned full speed. Numeric 7 is the real full speed
    // here (measured ~5.3k RPM), so the curve stays inside 1..7.
    if (cpu_temp_c >= FAN_FULL_TEMP_C) return 7; // full speed (numeric)
    if (cpu_temp_c <= FAN_CURVE_MIN_TEMP_C) return 1; // 0.2 -> level 1
    // Linear between (35 C, 0.2) and (70 C, 1.0), mapped onto the 0..7 steps.
    const double frac = FAN_CURVE_MIN_FRACTION +
                        (cpu_temp_c - FAN_CURVE_MIN_TEMP_C) /
                            (FAN_FULL_TEMP_C - FAN_CURVE_MIN_TEMP_C) *
                            (1.0 - FAN_CURVE_MIN_FRACTION);
    int lvl = static_cast<int>(frac * 7.0 + 0.5);
    if (lvl < 1) lvl = 1;
    if (lvl > 7) lvl = 7;
    return lvl;
}

int MitigationEngine::fan_level_for_temp_in_profile(double cpu_temp_c,
                                                    PowerProfileMode mode) noexcept {
    // REF-REQ-118.3: UltraEndurance is the only profile that stops the fan. It is
    // a cold-start exception, not a general weakening: above the threshold the
    // common curve applies unchanged, so the part can never be cooked to save a
    // few hundred milliwatts of fan power.
    if (mode == PowerProfileMode::UltraEndurance && cpu_temp_c > 0.0 &&
        cpu_temp_c <= FAN_ULTRA_STOP_TEMP_C) {
        return 0; // fan stopped
    }
    return fan_level_for_temp(cpu_temp_c);
}

int MitigationEngine::apply_fan_for_temp(double cpu_temp_c,
                                         PowerProfileMode mode) noexcept {
    ++g_fan_curve_application_count;
    const int lvl = fan_level_for_temp_in_profile(cpu_temp_c, mode);
    if (lvl < 0) return -1;

    // Write only on a level change: the observation cycle runs every 1-3 s and a
    // per-cycle fan write would be a needless sysfs storm.
    if (lvl == g_last_fan_level) return lvl;

    char level[16];
    // REF-REQ-114 (defect fix): always the numeric step, including the top one.
    // "full-speed" is a thinkpad_acpi keyword that resolves to `disengaged` on
    // this host - see fan_level_for_temp(). Writing 7 is the real full speed.
    std::snprintf(level, sizeof(level), "%d", lvl);
    if (!set_fan_level(level)) return -1;
    g_last_fan_level = lvl;
    return lvl;
}

uint64_t MitigationEngine::fan_curve_application_count() noexcept {
    return g_fan_curve_application_count;
}

bool MitigationEngine::set_fan_level(const char* level) noexcept {
    if (!level || *level == '\0') return false;

    // Capture the pre-boost level once, so restore puts back what the EC or the
    // user had rather than a hard-coded "auto".
    if (!s_hardware_baseline.fan_level_modified) {
        int rfd = ::open("/proc/acpi/ibm/fan", O_RDONLY | O_CLOEXEC);
        if (rfd >= 0) {
            char buf[256];
            ssize_t n = ::read(rfd, buf, sizeof(buf) - 1);
            ::close(rfd);
            if (n > 0) {
                buf[n] = '\0';
                const char* lv = std::strstr(buf, "level:");
                if (lv != nullptr) {
                    lv += 6;
                    while (*lv == ' ' || *lv == '\t') ++lv;
                    size_t i = 0;
                    while (i < sizeof(s_hardware_baseline.fan_level_baseline) - 1 &&
                           lv[i] != '\0' && lv[i] != '\n' && lv[i] != '\r' && lv[i] != ' ') {
                        s_hardware_baseline.fan_level_baseline[i] = lv[i];
                        ++i;
                    }
                    s_hardware_baseline.fan_level_baseline[i] = '\0';
                    if (i == 0) std::strncpy(s_hardware_baseline.fan_level_baseline, "auto", 15);
                }
            }
        }
    }

    int fd = hw_open_write("/proc/acpi/ibm/fan", O_WRONLY | O_CLOEXEC);
    if (fd < 0) return false;
    char buf[48];
    int len = std::snprintf(buf, sizeof(buf), "level %s\n", level);
    ssize_t w = ::write(fd, buf, static_cast<size_t>(len));
    ::close(fd);
    if (w > 0) {
        s_hardware_baseline.fan_level_modified = true;
        return true;
    }
    return false;
}

bool MitigationEngine::restore_fan_level() noexcept {
    if (!s_hardware_baseline.fan_level_modified) return true;
    const char* lv = (s_hardware_baseline.fan_level_baseline[0] != '\0')
                         ? s_hardware_baseline.fan_level_baseline
                         : "auto";
    int fd = hw_open_write("/proc/acpi/ibm/fan", O_WRONLY | O_CLOEXEC);
    if (fd < 0) return false;
    char buf[48];
    int len = std::snprintf(buf, sizeof(buf), "level %s\n", lv);
    ssize_t w = ::write(fd, buf, static_cast<size_t>(len));
    ::close(fd);
    if (w > 0) {
        s_hardware_baseline.fan_level_modified = false;
        // The EC owns the fan again; forget the last written level so the curve
        // re-applies on the next Performance/Balanced cycle even at the same
        // temperature.
        g_last_fan_level = -1;
    }
    return (w > 0);
}

// ---------------------------------------------------------------------------
// REF-REQ-115: SMU thermal/power limits via ryzenadj (optional tool)
// ---------------------------------------------------------------------------
static const char* find_ryzenadj() noexcept {
    static const char* cached = nullptr;
    static bool checked = false;
    if (checked) return cached;
    checked = true;
    const char* const candidates[] = {
        "/usr/local/bin/ryzenadj",
        "/usr/bin/ryzenadj",
    };
    for (const char* c : candidates) {
        if (::access(c, X_OK) == 0) {
            cached = c;
            break;
        }
    }
    return cached;
}

bool MitigationEngine::ryzenadj_available() noexcept {
    return find_ryzenadj() != nullptr;
}

bool MitigationEngine::apply_smu_performance_limits() noexcept {
    if (s_actuation_sandbox) return false;
    const char* tool = find_ryzenadj();
    if (tool == nullptr) return false;
    // Refuse to raise a limit that could not be put back. If the bootstrap
    // capture did not read real values (tool absent, or no permission to the SMU
    // table), raising the ceiling would create exactly the orphaned-actuation
    // state REF-REQ-112 had to repair for cpufreq. Leave the firmware alone.
    if (s_hardware_baseline.smu_stapm_mw == 0 || s_hardware_baseline.smu_tctl_c == 0) {
        return false;
    }

    char cmd[320];
    std::snprintf(cmd, sizeof(cmd),
                  "%s --tctl-temp=%u --stapm-limit=%u --fast-limit=%u --slow-limit=%u "
                  "--apu-slow-limit=%u >/dev/null 2>&1",
                  tool, SMU_TCTL_PERF_C, SMU_STAPM_PERF_MW, SMU_FAST_PERF_MW,
                  SMU_SLOW_PERF_MW, SMU_SLOW_PERF_MW);
    if (hw_system(cmd) != 0) return false;
    s_hardware_baseline.smu_limits_modified = true;
    return true;
}

bool MitigationEngine::restore_smu_limits() noexcept {
    if (!s_hardware_baseline.smu_limits_modified) return true;
    const char* tool = find_ryzenadj();
    // Without the tool we cannot restore, but clearing the flag would hide that;
    // keep it set so a later cycle with the tool present can still restore.
    if (tool == nullptr) return false;
    if (s_hardware_baseline.smu_stapm_mw == 0 || s_hardware_baseline.smu_tctl_c == 0) {
        s_hardware_baseline.smu_limits_modified = false;
        return false; // nothing captured to restore
    }

    const uint32_t apu_slow = (s_hardware_baseline.smu_apu_slow_mw != 0)
                                  ? s_hardware_baseline.smu_apu_slow_mw
                                  : s_hardware_baseline.smu_slow_mw;

    // Restore only the fields that were actually captured, so an absent row can
    // never be written back as a literal zero limit.
    char cmd[320];
    int n = std::snprintf(cmd, sizeof(cmd), "%s --tctl-temp=%u",
                          tool, s_hardware_baseline.smu_tctl_c);
    struct Field { const char* flag; uint32_t value; };
    const Field fields[] = {
        {" --stapm-limit=", s_hardware_baseline.smu_stapm_mw},
        {" --fast-limit=", s_hardware_baseline.smu_fast_mw},
        {" --slow-limit=", s_hardware_baseline.smu_slow_mw},
        {" --apu-slow-limit=", apu_slow},
    };
    for (const auto& f : fields) {
        if (f.value == 0 || n <= 0 || static_cast<size_t>(n) >= sizeof(cmd)) continue;
        n += std::snprintf(cmd + n, sizeof(cmd) - static_cast<size_t>(n), "%s%u", f.flag, f.value);
    }
    if (n > 0 && static_cast<size_t>(n) < sizeof(cmd)) {
        std::snprintf(cmd + n, sizeof(cmd) - static_cast<size_t>(n), " >/dev/null 2>&1");
    }
    if (hw_system(cmd) != 0) return false;
    s_hardware_baseline.smu_limits_modified = false;
    return true;
}

static bool execute_user_desktop_cmd(const char* cmd_body) noexcept;

bool MitigationEngine::set_bluetooth_blocked(bool block) noexcept {
    const char* val = block ? "1\n" : "0\n";
    bool any = false;
    char buf[32];
    for (int r = 0; r < 16; ++r) {
        char type_path[64];
        std::snprintf(type_path, sizeof(type_path), "/sys/class/rfkill/rfkill%d/type", r);
        size_t n = 0;
        if (core::fs::read_small_file(type_path, buf, sizeof(buf) - 1, &n) && n > 0) {
            if (std::strncmp(buf, "bluetooth", 9) == 0) {
                char soft_path[64];
                std::snprintf(soft_path, sizeof(soft_path), "/sys/class/rfkill/rfkill%d/soft", r);
                int fd = hw_open_write(soft_path, O_WRONLY | O_CLOEXEC);
                if (fd >= 0) {
                    if (::write(fd, val, 2) > 0) any = true;
                    ::close(fd);
                }
            }
        }
    }
    if (!block) {
        // Guarantee adapter power on when unblocked (REF-REQ-065)
        execute_user_desktop_cmd("bluetoothctl power on 2>/dev/null &");
    }
    return any;
}

bool MitigationEngine::cap_display_backlight(double max_pct) noexcept {
    const char* const bl_dirs[] = {
        "/sys/class/backlight/amdgpu_bl1",
        "/sys/class/backlight/amdgpu_bl0",
        "/sys/class/backlight/intel_backlight"
    };
    for (const char* bdir : bl_dirs) {
        char bpath[128];
        char mpath[128];
        std::snprintf(bpath, sizeof(bpath), "%s/brightness", bdir);
        std::snprintf(mpath, sizeof(mpath), "%s/max_brightness", bdir);
        char buf[32];
        size_t n = 0;
        if (core::fs::read_small_file(mpath, buf, sizeof(buf) - 1, &n) && n > 0) {
            uint32_t max_b = static_cast<uint32_t>(std::strtoul(buf, nullptr, 10));
            if (max_b == 0) continue;
            n = 0;
            if (core::fs::read_small_file(bpath, buf, sizeof(buf) - 1, &n) && n > 0) {
                uint32_t cur_b = static_cast<uint32_t>(std::strtoul(buf, nullptr, 10));
                if (!s_hardware_baseline.backlight_capped) {
                    s_hardware_baseline.backlight_brightness = cur_b;
                    s_hardware_baseline.backlight_max = max_b;
                }
                uint32_t cap_b = static_cast<uint32_t>(max_b * (std::clamp(max_pct, 10.0, 100.0) / 100.0));
                if (cur_b > cap_b) {
                    int fd = hw_open_write(bpath, O_WRONLY | O_CLOEXEC);
                    if (fd >= 0) {
                        char out[32];
                        int len = std::snprintf(out, sizeof(out), "%u\n", cap_b);
                        (void)::write(fd, out, static_cast<size_t>(len));
                        ::close(fd);
                        s_hardware_baseline.backlight_capped = true;
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool MitigationEngine::restore_display_backlight() noexcept {
    if (!s_hardware_baseline.backlight_capped || s_hardware_baseline.backlight_brightness == 0) {
        return false;
    }
    const char* const bl_dirs[] = {
        "/sys/class/backlight/amdgpu_bl1",
        "/sys/class/backlight/amdgpu_bl0",
        "/sys/class/backlight/intel_backlight"
    };
    for (const char* bdir : bl_dirs) {
        char bpath[128];
        std::snprintf(bpath, sizeof(bpath), "%s/brightness", bdir);
        int fd = hw_open_write(bpath, O_WRONLY | O_CLOEXEC);
        if (fd >= 0) {
            char out[32];
            int len = std::snprintf(out, sizeof(out), "%u\n", s_hardware_baseline.backlight_brightness);
            (void)::write(fd, out, static_cast<size_t>(len));
            ::close(fd);
            s_hardware_baseline.backlight_capped = false;
            return true;
        }
    }
    return false;
}

// Identity of the graphical session the daemon should drop privileges into.
// This used to be hardcoded as uid/gid 1000 with XDG_RUNTIME_DIR=/run/user/1000
// and WAYLAND_DISPLAY=wayland-0, so on any host whose desktop user is not
// uid 1000 the root daemon dropped into an unrelated account and ran commands
// as them.
struct DesktopSession {
    uid_t uid{0};
    gid_t gid{0};
    char  runtime_dir[64]{};
    char  wayland_display[32]{};
    bool  valid{false};
};

// REF-REQ-071 privilege-path audit: WAYLAND_DISPLAY and XDG_RUNTIME_DIR are
// interpolated into a /bin/sh command line that this root daemon executes. A
// user owns their /run/user/<uid> directory, so the compositor socket name is
// attacker-controlled: a file named "wayland-0;chmod 4755 /bin/bash;#" would
// otherwise be handed verbatim to a root shell and run as root. Accept only the
// characters a real Wayland socket component uses.
[[nodiscard]] static bool is_shell_safe_component(const char* s) noexcept {
    if (!s || *s == '\0') return false;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p; ++p) {
        const unsigned char c = *p;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-') {
            continue;
        }
        return false;
    }
    return true;
}

// Public wrappers so the Oracle Gate can assert the rejection directly.
bool MitigationEngine::is_safe_wayland_component(const char* s) noexcept {
    return is_shell_safe_component(s);
}

bool MitigationEngine::is_safe_run_user_dir(const char* s) noexcept {
    constexpr char prefix[] = "/run/user/";
    constexpr size_t plen = sizeof(prefix) - 1;
    if (!s || std::strncmp(s, prefix, plen) != 0) return false;
    const char* d = s + plen;
    if (*d == '\0') return false;
    for (; *d; ++d) {
        if (*d < '0' || *d > '9') return false;
    }
    return true;
}

// XDG_RUNTIME_DIR must be exactly /run/user/<digits>, never a path that could
// escape /run/user or smuggle shell syntax into the command line.
[[nodiscard]] static bool is_run_user_dir(const char* s) noexcept {
    constexpr char prefix[] = "/run/user/";
    constexpr size_t plen = sizeof(prefix) - 1;
    if (!s || std::strncmp(s, prefix, plen) != 0) return false;
    const char* d = s + plen;
    if (*d == '\0') return false;
    for (; *d; ++d) {
        if (*d < '0' || *d > '9') return false;
    }
    return true;
}

static DesktopSession detect_desktop_session() noexcept {
    DesktopSession best{};
    DesktopSession fallback{};

    DIR* dir = ::opendir("/run/user");
    if (!dir) return best;

    struct dirent* entry = nullptr;
    while ((entry = ::readdir(dir)) != nullptr) {
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9') continue;

        // The directory name becomes a shell word below; accept digits only so
        // a crafted entry such as "1000;id" cannot survive into the command.
        bool digits_only = true;
        for (const char* d = entry->d_name; *d; ++d) {
            if (*d < '0' || *d > '9') {
                digits_only = false;
                break;
            }
        }
        if (!digits_only) continue;

        DesktopSession cand{};
        int n = std::snprintf(cand.runtime_dir, sizeof(cand.runtime_dir), "/run/user/%s", entry->d_name);
        if (n <= 0 || static_cast<size_t>(n) >= sizeof(cand.runtime_dir)) continue;

        struct stat st{};
        if (::stat(cand.runtime_dir, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        if (st.st_uid < 1000) continue; // skip system accounts

        cand.uid = st.st_uid;
        cand.gid = st.st_gid;
        std::strncpy(cand.wayland_display, "wayland-0", sizeof(cand.wayland_display) - 1);
        cand.valid = true;
        if (!fallback.valid) fallback = cand;

        // Prefer a runtime dir that actually carries a compositor socket.
        DIR* rt = ::opendir(cand.runtime_dir);
        if (!rt) continue;
        bool has_compositor = false;
        struct dirent* sock = nullptr;
        while ((sock = ::readdir(rt)) != nullptr) {
            if (std::strncmp(sock->d_name, "wayland-", 8) != 0) continue;
            if (std::strstr(sock->d_name, ".lock") != nullptr) continue;
            if (!is_shell_safe_component(sock->d_name)) continue;
            std::strncpy(cand.wayland_display, sock->d_name, sizeof(cand.wayland_display) - 1);
            cand.wayland_display[sizeof(cand.wayland_display) - 1] = '\0';
            has_compositor = true;
            break;
        }
        ::closedir(rt);

        if (has_compositor) {
            best = cand;
            break;
        }
    }
    ::closedir(dir);

    return best.valid ? best : fallback;
}

static bool execute_user_desktop_cmd(const char* cmd_body) noexcept {
    if (!cmd_body) return false;
    // REF-REQ-071 & REF-ARCH-048: Test harness isolation guard.
    // Prevents automated tests from interfering with physical user display / compositor.
    if (::getenv("WATTCURB_TEST_MOCK_DESKTOP") != nullptr) {
        return true;
    }

    // The body is wrapped in single quotes below, so a quote inside it would
    // escape that quoting and inject into a root-spawned shell. Every current
    // caller passes a string literal; refuse rather than depend on that.
    if (std::strchr(cmd_body, '\'') != nullptr) return false;

    const DesktopSession session = detect_desktop_session();
    if (!session.valid) return false;

    // Defense in depth: even if detection changes, nothing reaches the shell
    // that is not a plain Wayland socket name or a /run/user/<digits> path.
    if (!is_shell_safe_component(session.wayland_display) || !is_run_user_dir(session.runtime_dir)) {
        return false;
    }

    char cmd[512];
    int n;
    if (::geteuid() == 0) {
        n = std::snprintf(cmd, sizeof(cmd),
            "export WAYLAND_DISPLAY=%s XDG_RUNTIME_DIR=%s; "
            "setpriv --reuid=%u --regid=%u --clear-groups sh -c '%s' >/dev/null 2>&1 &",
            session.wayland_display, session.runtime_dir,
            static_cast<unsigned>(session.uid), static_cast<unsigned>(session.gid),
            cmd_body);
    } else {
        n = std::snprintf(cmd, sizeof(cmd),
            "export WAYLAND_DISPLAY=%s XDG_RUNTIME_DIR=%s; ( %s ) >/dev/null 2>&1 &",
            session.wayland_display, session.runtime_dir, cmd_body);
    }

    // Truncation would cut the command mid-quote; never hand that to a shell.
    if (n < 0 || static_cast<size_t>(n) >= sizeof(cmd)) return false;

    return (hw_system(cmd) == 0);
}

bool MitigationEngine::set_display_refresh_rate(uint32_t hz) noexcept {
    // REF-REQ-071 & REF-ARCH-048: Idempotent guard to eliminate DRM modeset blackout
    bool target_drrs = (hz <= 50);
    if (s_hardware_baseline.drrs_applied == target_drrs) {
        return true; // Already in target mode; prevent redundant modeset flicker
    }
    s_hardware_baseline.drrs_applied = target_drrs;
    if (target_drrs) {
        return execute_user_desktop_cmd("kscreen-doctor output.1.mode.2");
    } else {
        return execute_user_desktop_cmd("kscreen-doctor output.1.mode.1");
    }
}

bool MitigationEngine::set_kwin_effects_suspended(bool suspend) noexcept {
    // REF-REQ-071 & REF-ARCH-048: Idempotent guard to eliminate compositor shader rebuild
    if (s_hardware_baseline.kwin_blur_unloaded == suspend) {
        return true; // Already in target state; prevent redundant effect toggling
    }
    s_hardware_baseline.kwin_blur_unloaded = suspend;
    if (suspend) {
        return execute_user_desktop_cmd("qdbus6 org.kde.KWin /Effects unloadEffect blur");
    } else {
        return execute_user_desktop_cmd("qdbus6 org.kde.KWin /Effects loadEffect blur");
    }
}

bool MitigationEngine::set_baloo_suspended(bool suspend) noexcept {
    // REF-REQ-071 & REF-ARCH-048: Idempotent guard for baloo state
    if (s_hardware_baseline.baloo_suspended == suspend) {
        return true; // Already in target state
    }
    s_hardware_baseline.baloo_suspended = suspend;
    if (suspend) {
        return execute_user_desktop_cmd("balooctl6 suspend 2>/dev/null || balooctl suspend 2>/dev/null");
    } else {
        return execute_user_desktop_cmd("balooctl6 resume 2>/dev/null || balooctl resume 2>/dev/null");
    }
}

bool MitigationEngine::set_wifi_txpower_limit(uint32_t mbm) noexcept {
    char ifname[32];
    if (!detect_wireless_ifname(ifname, sizeof(ifname))) return false;

    // The Oracle Gate verifies the cap/restore state machine rather than the
    // radio, so a sandboxed run reports the actuation as performed (matching the
    // previous hw_system() semantics) without emitting the netlink command.
    const bool ok = s_actuation_sandbox || nl80211_set_tx_power(ifname, false, mbm);
    s_hardware_baseline.wifi_txpower_capped = true;
    return ok;
}

bool MitigationEngine::restore_wifi_txpower() noexcept {
    if (!s_hardware_baseline.wifi_txpower_capped) return false;

    char ifname[32];
    if (!detect_wireless_ifname(ifname, sizeof(ifname))) {
        // Radio vanished (dongle unplugged): drop the override so the flag
        // cannot wedge every subsequent baseline restore.
        s_hardware_baseline.wifi_txpower_capped = false;
        return false;
    }

    const bool ok = s_actuation_sandbox || nl80211_set_tx_power(ifname, true, 0);
    s_hardware_baseline.wifi_txpower_capped = false;
    return ok;
}

// Implements REF-REQ-087 & REF-ARCH-064: Stack-allocated sysctl writer without heap allocation
static bool write_uint32_sysctl(const char* path, uint32_t val) noexcept {
    if (!path) return false;
    int fd = hw_open_write(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) return false;
    char buf[32];
    int len = std::snprintf(buf, sizeof(buf), "%u\n", val);
    if (len <= 0) {
        ::close(fd);
        return false;
    }
    ssize_t w = ::write(fd, buf, static_cast<size_t>(len));
    ::close(fd);
    return (w > 0);
}

bool MitigationEngine::set_vm_dirty_writeback_centisecs(uint32_t centisecs) noexcept {
    return write_uint32_sysctl("/proc/sys/vm/dirty_writeback_centisecs", centisecs);
}

bool MitigationEngine::set_vm_dirty_expire_centisecs(uint32_t centisecs) noexcept {
    return write_uint32_sysctl("/proc/sys/vm/dirty_expire_centisecs", centisecs);
}

bool MitigationEngine::set_vm_laptop_mode(uint32_t mode) noexcept {
    return write_uint32_sysctl("/proc/sys/vm/laptop_mode", mode);
}

bool MitigationEngine::restore_vm_writeback_baseline() noexcept {
    if (!s_hardware_baseline.captured) return false;
    set_vm_dirty_writeback_centisecs(s_hardware_baseline.vm_dirty_writeback_centisecs);
    set_vm_dirty_expire_centisecs(s_hardware_baseline.vm_dirty_expire_centisecs);
    set_vm_laptop_mode(s_hardware_baseline.vm_laptop_mode);
    s_hardware_baseline.vm_writeback_modified = false;
    return true;
}

void MitigationEngine::trigger_3tier_vram_gc() noexcept {
    WATTCURB_PROFILE_SCOPE("mitig.3tier_vram_gc");
    // Tier 1: Unload KWin blur shader effects (releases ~100MB-250MB VRAM)
    set_kwin_effects_suspended(true);

    // Tier 2: Chromium/Electron GPU discardable memory cache eviction via memory.reclaim
    DIR* proc_dir = ::opendir("/proc");
    if (proc_dir) {
        struct dirent* entry = nullptr;
        while ((entry = ::readdir(proc_dir)) != nullptr) {
            if (entry->d_type != DT_DIR && entry->d_type != DT_UNKNOWN) continue;
            if (entry->d_name[0] < '0' || entry->d_name[0] > '9') continue;
            int32_t pid = std::atoi(entry->d_name);
            if (pid <= 1) continue;

            char comm_path[64];
            std::snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", pid);
            int fd = ::open(comm_path, O_RDONLY | O_CLOEXEC);
            if (fd >= 0) {
                char comm_buf[32];
                ssize_t n = ::read(fd, comm_buf, sizeof(comm_buf) - 1);
                ::close(fd);
                if (n > 0) {
                    if (comm_buf[n - 1] == '\n') --n;
                    comm_buf[n] = '\0';
                    std::string_view comm(comm_buf, static_cast<size_t>(n));
                    if (comm == "chrome" || comm == "chromium" || comm == "msedge" ||
                        comm == "code" || comm == "slack" || comm == "discord" ||
                        comm == "chatgpt" || comm == "electron") {
                        // Request 100MB cgroups v2 memory reclaim
                        // REF-REQ-112: file-only while the guard holds swap back.
                        apply_memory_reclaim(pid, 100ULL * 1024 * 1024,
                                             MemoryPressureGuard::swap_feeding_suspended());
                    }
                }
            }
        }
        ::closedir(proc_dir);
    }

    // REF-REQ-098: Tier 3 previously wrote "3" to /proc/sys/vm/drop_caches.
    //
    // That purges the ENTIRE page cache, dentry cache and inode cache system
    // wide. The kernel walks and frees every cached page while holding locks, and
    // afterwards every binary, library and file the desktop touches has to be
    // re-read from storage. On this machine that is gigabytes of cache - the
    // machine stops responding for as long as it takes, which is exactly the
    // minute-long freeze this mode was reported to cause.
    //
    // It also saves no power: page cache occupies otherwise-free memory, and
    // forcing the re-reads costs additional storage and CPU energy. The write is
    // removed outright rather than made conditional; there is no battery state in
    // which purging the system's caches is the right move.
}

bool MitigationEngine::pci_class_allows_runtime_pm(uint32_t pci_class, bool audio_active) noexcept {
    // sysfs reports 0xCCSSPP - CC class, SS subclass, PP prog-if.
    const uint32_t cc = (pci_class >> 16) & 0xFFu;
    const uint32_t ss = (pci_class >> 8) & 0xFFu;

    if (cc == 0x02) return true;              // network: built for runtime PM
    if (cc == 0x08 && ss == 0x05) return true; // SD/MMC host: idle card reader
    if (cc == 0x04) return !audio_active;      // multimedia, only while silent

    // Everything else is infrastructure and is never suspended:
    //   0x01 storage      - I/O stalls
    //   0x03 display      - the screen stops updating
    //   0x06 bridges      - takes everything downstream with it
    //   0x0c03 USB host   - keyboard and mouse stop responding
    //   0x0806 IOMMU      - core platform
    return false;
}

void MitigationEngine::apply_pcie_runtime_pm_auto() noexcept {
    WATTCURB_PROFILE_SCOPE("mitig.pcie_runtime_pm");

    // REF-REQ-098: LIVENESS INVARIANT.
    //
    // This previously wrote "auto" to power/control for EVERY PCI device with no
    // exclusion whatsoever. On this class of machine that set includes the USB
    // host controller (input dies), the display controller (screen stops
    // updating), the NVMe controller (I/O stalls), the PCI bridges (everything
    // downstream of them suspends) and the IOMMU. The result is indistinguishable
    // from a hung system.
    //
    // Runtime PM is therefore an ALLOWLIST, not a denylist: a class has to be
    // known-safe to be suspended, rather than merely not yet known to be fatal.
    DIR* dir = ::opendir("/sys/bus/pci/devices");
    if (!dir) return;

    struct dirent* entry = nullptr;
    while ((entry = ::readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;

        char class_path[256];
        std::snprintf(class_path, sizeof(class_path),
                      "/sys/bus/pci/devices/%s/class", entry->d_name);
        char class_buf[32];
        size_t n = 0;
        if (!core::fs::read_small_file(class_path, class_buf, sizeof(class_buf) - 1, &n) || n == 0) {
            continue; // unknown class: leave it alone
        }
        class_buf[n] = '\0';

        const auto full = static_cast<uint32_t>(std::strtoul(class_buf, nullptr, 16));
        if (!pci_class_allows_runtime_pm(full, s_audio_state.active)) continue;

        char ctrl_path[256];
        std::snprintf(ctrl_path, sizeof(ctrl_path),
                      "/sys/bus/pci/devices/%s/power/control", entry->d_name);
        int fd = hw_open_write(ctrl_path, O_WRONLY | O_CLOEXEC);
        if (fd >= 0) {
            (void)::write(fd, "auto\n", 5);
            ::close(fd);
        }
    }
    ::closedir(dir);
}

void MitigationEngine::apply_usb_runtime_pm_auto() noexcept {
    WATTCURB_PROFILE_SCOPE("mitig.usb_runtime_pm");
    DIR* dir = ::opendir("/sys/bus/usb/devices");
    if (!dir) return;

    struct dirent* entry = nullptr;
    while ((entry = ::readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;

        // Skip usb interface child directories (e.g. 1-1:1.0), only target USB devices
        if (std::strchr(entry->d_name, ':') != nullptr) {
            continue;
        }

        char dev_path[256];
        std::snprintf(dev_path, sizeof(dev_path), "/sys/bus/usb/devices/%s", entry->d_name);

        // Check if device is HID (03) or Bluetooth (e0) to preserve responsiveness and Bluetooth invariant (REF-REQ-065)
        char class_path[320];
        std::snprintf(class_path, sizeof(class_path), "%s/bDeviceClass", dev_path);
        char class_buf[16];
        size_t n = 0;
        bool is_immune = false;
        if (core::fs::read_small_file(class_path, class_buf, sizeof(class_buf) - 1, &n) && n > 0) {
            class_buf[n] = '\0';
            if (std::strstr(class_buf, "03") || std::strstr(class_buf, "e0")) {
                is_immune = true;
            }
        }

        // Also inspect interface subdirectories for HID (03) or Bluetooth (e0)
        if (!is_immune) {
            DIR* sub_dir = ::opendir(dev_path);
            if (sub_dir) {
                struct dirent* sub_entry = nullptr;
                while ((sub_entry = ::readdir(sub_dir)) != nullptr) {
                    if (std::strchr(sub_entry->d_name, ':') != nullptr) {
                        char if_class_path[384];
                        std::snprintf(if_class_path, sizeof(if_class_path), "%s/%s/bInterfaceClass", dev_path, sub_entry->d_name);
                        char if_buf[16];
                        size_t if_n = 0;
                        if (core::fs::read_small_file(if_class_path, if_buf, sizeof(if_buf) - 1, &if_n) && if_n > 0) {
                            if_buf[if_n] = '\0';
                            if (std::strstr(if_buf, "03") || std::strstr(if_buf, "e0")) {
                                is_immune = true;
                                break;
                            }
                        }
                    }
                }
                ::closedir(sub_dir);
            }
        }

        if (is_immune) {
            continue; // Skip HID input and Bluetooth devices
        }

        char ctrl_path[320];
        std::snprintf(ctrl_path, sizeof(ctrl_path), "%s/power/control", dev_path);
        int fd = hw_open_write(ctrl_path, O_WRONLY | O_CLOEXEC);
        if (fd >= 0) {
            (void)::write(fd, "auto\n", 5);
            ::close(fd);
        }
    }
    ::closedir(dir);
}

bool MitigationEngine::set_audio_codec_power_save(int seconds, bool controller) noexcept {
    WATTCURB_PROFILE_SCOPE("mitig.audio_codec_power_save");
    bool ok = false;
    int fd = hw_open_write("/sys/module/snd_hda_intel/parameters/power_save", O_WRONLY | O_CLOEXEC);
    if (fd >= 0) {
        char buf[16];
        int len = std::snprintf(buf, sizeof(buf), "%d\n", seconds);
        if (::write(fd, buf, static_cast<size_t>(len)) > 0) {
            ok = true;
        }
        ::close(fd);
    }

    int c_fd = hw_open_write("/sys/module/snd_hda_intel/parameters/power_save_controller", O_WRONLY | O_CLOEXEC);
    if (c_fd >= 0) {
        const char* val = controller ? "Y\n" : "N\n";
        (void)::write(c_fd, val, 2);
        ::close(c_fd);
    }

    s_hardware_baseline.audio_power_save_modified = true;
    return ok;
}

bool MitigationEngine::restore_audio_codec_baseline() noexcept {
    WATTCURB_PROFILE_SCOPE("mitig.restore_audio_codec");
    if (!s_hardware_baseline.audio_power_save_modified) return true;

    if (s_hardware_baseline.audio_power_save >= 0) {
        int fd = hw_open_write("/sys/module/snd_hda_intel/parameters/power_save", O_WRONLY | O_CLOEXEC);
        if (fd >= 0) {
            char buf[16];
            int len = std::snprintf(buf, sizeof(buf), "%d\n", s_hardware_baseline.audio_power_save);
            (void)::write(fd, buf, static_cast<size_t>(len));
            ::close(fd);
        }
    }

    if (s_hardware_baseline.audio_power_save_controller[0] != '\0') {
        int c_fd = hw_open_write("/sys/module/snd_hda_intel/parameters/power_save_controller", O_WRONLY | O_CLOEXEC);
        if (c_fd >= 0) {
            char buf[16];
            int len = std::snprintf(buf, sizeof(buf), "%s\n", s_hardware_baseline.audio_power_save_controller);
            (void)::write(c_fd, buf, static_cast<size_t>(len));
            ::close(c_fd);
        }
    }

    s_hardware_baseline.audio_power_save_modified = false;
    return true;
}

void MitigationEngine::rollback_all() noexcept {
    WATTCURB_PROFILE_SCOPE("mitig.rollback_all");
    // Implements REF-REQ-031 Sec 3.2, REF-REQ-049 & REF-REQ-055: Restore all mitigated processes faithfully and heal audio stack
    audit_and_heal_audio_stack();

    for (const auto& tm : m_tracked) {
        if (tm.affinity_capped) {
            restore_core_affinity(tm.pid, &tm.original_affinity);
        }
        if (tm.sched_batch_applied || tm.sched_idle_applied ||
            tm.current_action == MitigationAction::SchedIdle ||
            tm.current_action == MitigationAction::CgroupFreeze) {
            if (tm.current_action == MitigationAction::CgroupFreeze) {
                apply_cgroup_freeze(tm.pid, false);
            }
            restore_sched_normal(tm.pid, tm.original_sched_policy, tm.original_nice);
        }
        if (tm.original_timerslack_ns > 0) {
            apply_timer_slack(tm.pid, tm.original_timerslack_ns);
        }
    }
    m_tracked.clear();

    if (m_aspm_modified) {
        set_pcie_aspm_policy(s_hardware_baseline.aspm_policy);
        m_aspm_modified = false;
    }
    if (m_backlight_capped) {
        restore_display_backlight();
        m_backlight_capped = false;
    }
    if (s_hardware_baseline.vm_writeback_modified) {
        restore_vm_writeback_baseline();
    }
    if (s_hardware_baseline.captured) {
        restore_hardware_baseline();
    }
}

void MitigationEngine::thaw_all_frozen() noexcept {
    WATTCURB_PROFILE_SCOPE("mitig.thaw_frozen");
    // Transition from UltraEndurance to PowerSaver: thaw cgroups and downgrade to SCHED_IDLE
    for (auto& tm : m_tracked) {
        if (tm.current_action == MitigationAction::CgroupFreeze) {
            apply_cgroup_freeze(tm.pid, false);
            apply_sched_idle(tm.pid);
            tm.current_action = MitigationAction::SchedIdle;
        }
    }
    if (m_backlight_capped) {
        restore_display_backlight();
        m_backlight_capped = false;
    }
}

ActiveMitigationStatus MitigationEngine::evaluate_and_actuate(
    AnalysisReportData& report,
    bool on_battery,
    double battery_pct
) noexcept {
    WATTCURB_PROFILE_SCOPE("mitig.evaluate_actuate");
    // Implements REF-REQ-049: Guarantee audio stack is immune and healthy every cycle
    audit_and_heal_audio_stack();

    ActiveMitigationStatus status{};

    // 1. Determine profile governed by state machine & hysteresis (REF-REQ-031 Sec 2.1)
    PowerProfileMode target_profile = determine_profile(on_battery, battery_pct);
    PowerProfileMode old_profile = m_current_profile;

    if (old_profile != target_profile) {
        // REF-REQ-092.2: process-domain mitigations must be released BEFORE the
        // new profile actuates. rollback_all() reaches restore_hardware_baseline(),
        // so running it afterwards would immediately undo the just-applied
        // full-silicon unleash (and, for Balanced, reset the profile to the
        // bootstrap baseline). Restore first, then apply.
        if (target_profile == PowerProfileMode::Performance ||
            target_profile == PowerProfileMode::Balanced) {
            rollback_all();
        }

        apply_power_profile(target_profile);

        if (old_profile == PowerProfileMode::UltraEndurance && target_profile == PowerProfileMode::PowerSaver) {
            thaw_all_frozen();
        } else if (target_profile == PowerProfileMode::PowerSaver) {
            m_aspm_modified = true;
        } else if (target_profile == PowerProfileMode::UltraEndurance) {
            m_aspm_modified = true;
            cap_display_backlight(35.0);
            m_backlight_capped = true;
        }
        m_current_profile = target_profile;
    }

    status.current_profile = m_current_profile;

    // Periodically shield all active desktop terminals & shells (REF-REQ-084, REF-ARCH-061)
    if (m_scan_counter++ % 10 == 0) {
        shield_all_interactive_terminals();
    }

    // In Performance mode: Active C1/C2 Cluster Dispersion & Interactive Terminal Shield (REF-REQ-084, REF-ARCH-061)
    if (m_current_profile == PowerProfileMode::Performance) {
        audit_and_heal_audio_stack();

        // 1. Proactively shield interactive terminals and shells
        for (const auto& proc : report.top_processes) {
            if (proc.pid <= 1) continue;
            auto tier = static_cast<ProcessSafetyTier>(proc.safety_tier);
            if (tier == ProcessSafetyTier::DesktopCore) {
                shield_interactive_process(proc.pid, proc.comm.view());
            }
        }

        // REF-REQ-104: Performance holds nothing back, so the C1/C2 dispersion is
        // NOT applied here. Confining a compile or render to one cluster (8 of
        // the 16 threads) and lowering it to SCHED_BATCH is a demotion, and the
        // mode whose contract is "do not hold anything back" must not throttle
        // the very workload the user started. Interactive latency is preserved
        // by the DesktopCore/terminal elevation above and by the active-window
        // guarantee, not by capping the heavy work. (The cluster topology and
        // is_heavy_compute_candidate() remain for the saving profiles and tests.)


        // (No de-escalation loop: nothing is confined in Performance, so there
        //  is no dispersion state to walk back.)


        if (status.feature_summary_count < status.feature_summaries.size()) {
            status.feature_summaries[status.feature_summary_count++] = "CPU: 4.1GHz Boost (Performance)";
        }
        if (status.feature_summary_count < status.feature_summaries.size()) {
            status.feature_summaries[status.feature_summary_count++] = "All-Core Compute: no affinity cap, no SCHED_IDLE";
        }
        if (status.feature_summary_count < status.feature_summaries.size()) {
            status.feature_summaries[status.feature_summary_count++] = "Terminal Latency Shield: Active (nice -5, C1)";
        }
        status.active_summary = "Performance Mode (All-Core Compute, Terminal Shield Active)";
        report.mitigation_status = status;
        return status;
    }

    // 2. Add hardware feature summaries
    if (m_current_profile == PowerProfileMode::PowerSaver) {
        if (status.feature_summary_count < status.feature_summaries.size()) {
            status.feature_summaries[status.feature_summary_count++] = "PCIe ASPM: powersave";
        }
        if (s_epp_applied && status.feature_summary_count < status.feature_summaries.size()) {
            status.feature_summaries[status.feature_summary_count++] = "CPU EPP: balance_power";
        }
    } else if (m_current_profile == PowerProfileMode::UltraEndurance) {
        if (status.feature_summary_count < status.feature_summaries.size()) {
            status.feature_summaries[status.feature_summary_count++] = "PCIe ASPM: powersave";
        }
        if (s_epp_applied && status.feature_summary_count < status.feature_summaries.size()) {
            status.feature_summaries[status.feature_summary_count++] = "CPU EPP: power";
        }
        if (status.feature_summary_count < status.feature_summaries.size()) {
            status.feature_summaries[status.feature_summary_count++] = "Display Panel: 35% Hard-Cap";
        }
        if (status.feature_summary_count < status.feature_summaries.size()) {
            status.feature_summaries[status.feature_summary_count++] = "Wi-Fi Tx: 12dBm Capped";
        }
    }

    // 3. Inspect top power culprits from attribution analysis
    for (auto& proc : report.top_processes) {
        if (proc.pid <= 1) continue;

        auto tier = static_cast<ProcessSafetyTier>(proc.safety_tier);

        // Proactive DesktopCore Shield (compositors, terminals, shells)
        if (tier == ProcessSafetyTier::DesktopCore) {
            shield_interactive_process(proc.pid, proc.comm.view());
            continue;
        }

        // Tier 0 (CriticalImmune) is strictly untouchable! (REF-REQ-031 Sec 3.1)
        if (tier == ProcessSafetyTier::CriticalImmune) {
            continue;
        }

        // REF-REQ-117 (DEF-2): Tier 3 user applications are input consumers. This
        // path (MitigationEngine::evaluate_and_actuate) is not the daemon's, but
        // the same rule must hold wherever mitigation is decided, or the two
        // ladders disagree about who may be throttled. See FeatureManager for the
        // full rationale and the ancestry fallback for Electron subprocesses.
        if (tier == ProcessSafetyTier::UserInteractive || FeatureManager::is_user_app_tree(proc.pid)) {
            continue;
        }

        // Tier 2 (DesktopShell): Only safe proactive memory reclaim allowed under progressive profile
        if (tier == ProcessSafetyTier::DesktopShell) {
            if (m_current_profile == PowerProfileMode::UltraEndurance && proc.pss_kib > 250 * 1024) {
                uint64_t reclaim_target = 64ULL * 1024 * 1024; // 64 MB
                if (apply_memory_reclaim(proc.pid, reclaim_target,
                                         MemoryPressureGuard::swap_feeding_suspended())) { // REF-REQ-112
                    status.reclaimed_bytes += reclaim_target;
                    status.estimated_savings_watts += 0.05;
                }
            }
            continue;
        }

        // Check if already tracked
        bool already_tracked = false;
        for (const auto& tm : m_tracked) {
            if (tm.pid == proc.pid) {
                already_tracked = true;
                break;
            }
        }

        // Mitigation candidate evaluation based on profile and WDI / wakeup score
        // REF-REQ-044: Non-Halting & Zero-Kill Invariant. Processes are NEVER frozen or killed!
        bool should_throttle = false;
        bool should_relax_timer = false;
        bool should_reclaim = false;

        if (tier == ProcessSafetyTier::BackgroundWorker) {
            // Background indexing/sync tasks (baloo, tracker, updatedb, etc.)
            if (m_current_profile == PowerProfileMode::UltraEndurance) {
                should_throttle = true;
                should_relax_timer = true;
                should_reclaim = (proc.pss_kib > 50 * 1024);
            } else if (m_current_profile == PowerProfileMode::PowerSaver) {
                should_throttle = (proc.wdi_score > 3.0 || proc.wakeups_per_sec > 50);
                should_relax_timer = (proc.wakeups_per_sec > 100);
                should_reclaim = (proc.pss_kib > 100 * 1024);
            } else { // Balanced
                should_throttle = (proc.wdi_score > 10.0 || proc.cpu_watts > 1.5);
                should_relax_timer = (proc.wakeups_per_sec > 250);
            }
        } else if (tier == ProcessSafetyTier::RunawayCandidate) {
            // General worker or runaway candidate - graceful throttling only, never killed
            if (proc.is_runaway_candidate || proc.wdi_score > 10.0) {
                should_throttle = true;
                should_relax_timer = true;
                if (m_current_profile == PowerProfileMode::UltraEndurance) {
                    should_reclaim = true;
                }
            }
        } else if (tier == ProcessSafetyTier::UserInteractive) {
            // Interactive apps (browser, terminal, editor)
            if (m_current_profile == PowerProfileMode::UltraEndurance && proc.wakeups_per_sec > 300) {
                should_relax_timer = true;
            }
            if (m_current_profile != PowerProfileMode::Balanced && proc.pss_kib > 500 * 1024 && proc.cpu_watts < 0.2) {
                should_reclaim = true;
            }
        }

        // Apply Actuations (Zero-Freeze: Only SchedIdle, TimerSlack, MemoryReclaim)
        if (should_throttle && !already_tracked) {
            // REF-REQ-055: the tracking table is what rollback_all() walks. If it
            // is full, applying a throttle would produce a mitigation the daemon
            // can never release, leaving the process at SCHED_IDLE even after it
            // exits. Refuse rather than leak an irreversible demotion.
            if (m_tracked.size() >= MAX_TRACKED_MITIGATIONS) {
                // Saturated: skip this candidate.
            } else {
                int orig_nice = ::getpriority(PRIO_PROCESS, static_cast<id_t>(proc.pid));
                int orig_sched = ::sched_getscheduler(proc.pid);
                cpu_set_t orig_aff;
                CPU_ZERO(&orig_aff);
                ::sched_getaffinity(proc.pid, sizeof(cpu_set_t), &orig_aff);

                if (apply_sched_idle(proc.pid)) {
                    ++status.throttled_count;
                    status.estimated_savings_watts += (proc.cpu_watts * 0.4);
                    m_tracked.push_back(TrackedMitigation{
                        .pid = proc.pid,
                        .tier = tier,
                        .current_action = MitigationAction::SchedIdle,
                        .applied_timestamp_sec = 0,
                        .original_nice = orig_nice,
                        .original_sched_policy = (orig_sched >= 0) ? orig_sched : SCHED_OTHER,
                        .original_timerslack_ns = proc.timerslack_ns,
                        .original_affinity = orig_aff,
                        .affinity_capped = false,
                        .sched_batch_applied = false,
                        .sched_idle_applied = true
                    });
                }
            }
        }

        if (should_relax_timer && proc.timerslack_ns < 100'000'000ULL) { // Relax to 100ms
            if (apply_timer_slack(proc.pid, 100'000'000ULL)) {
                status.estimated_savings_watts += (proc.wakeup_tax_watts * 0.5);
            }
        }

        if (should_reclaim) {
            uint64_t reclaim_amount = std::min(proc.pss_kib * 1024ULL / 2, 128ULL * 1024 * 1024);
            if (reclaim_amount > 0 && apply_memory_reclaim(proc.pid, reclaim_amount,
                                                           MemoryPressureGuard::swap_feeding_suspended())) { // REF-REQ-112
                status.reclaimed_bytes += reclaim_amount;
                status.estimated_savings_watts += 0.04;
            }
        }

        bool should_cap_affinity = false;
        if (tier != ProcessSafetyTier::CriticalImmune && tier != ProcessSafetyTier::DesktopCore && !is_immune_process(proc.pid)) {
            double cpu_w_threshold = 1.2;
            if (m_current_profile == PowerProfileMode::UltraEndurance) {
                cpu_w_threshold = 0.35; // Scaled to 4W TDP
            } else if (m_current_profile == PowerProfileMode::PowerSaver) {
                cpu_w_threshold = 0.70; // Scaled to 10W TDP
            } else if (m_current_profile == PowerProfileMode::Performance) {
                cpu_w_threshold = 2.0;
            }

            if (proc.cpu_watts > cpu_w_threshold) {
                should_cap_affinity = true;
            } else if (proc.num_threads >= 4 && proc.cpu_watts > 0.25) {
                // Multi-threaded parallel workload attempting to saturate cores
                should_cap_affinity = true;
            } else if (proc.wdi_score > 6.0 || proc.is_runaway_candidate) {
                should_cap_affinity = true;
            } else if (tier == ProcessSafetyTier::BackgroundWorker && proc.cpu_watts > 0.20) {
                should_cap_affinity = true;
            } else if (tier == ProcessSafetyTier::RunawayCandidate && proc.cpu_watts > 0.25) {
                should_cap_affinity = true;
            }
        }

        if (should_cap_affinity) {
            int orig_nice = ::getpriority(PRIO_PROCESS, static_cast<id_t>(proc.pid));
            int orig_sched = ::sched_getscheduler(proc.pid);
            cpu_set_t orig_aff;
            CPU_ZERO(&orig_aff);
            ::sched_getaffinity(proc.pid, sizeof(cpu_set_t), &orig_aff);

            const auto& topo = get_cluster_topology();
            cpu_set_t allowed_set;
            bool is_heavy = is_heavy_compute_candidate(proc);
            if (is_heavy && topo.cluster_count >= 2) {
                allowed_set = topo.c2_cpuset;
            } else {
                allowed_set = get_headroom_allowed_cpuset(m_current_profile);
            }

            if (apply_core_affinity_cap(proc.pid, &allowed_set)) {
                int nice_val = 10;
                if (m_current_profile == PowerProfileMode::UltraEndurance) nice_val = 15;
                else if (m_current_profile == PowerProfileMode::Performance) nice_val = 5;
                apply_sched_batch(proc.pid, nice_val);
                if (!already_tracked && m_tracked.size() < MAX_TRACKED_MITIGATIONS) {
                    m_tracked.push_back(TrackedMitigation{
                        .pid = proc.pid,
                        .tier = tier,
                        .current_action = MitigationAction::AffinityCap,
                        .applied_timestamp_sec = 0,
                        .original_nice = orig_nice,
                        .original_sched_policy = (orig_sched >= 0) ? orig_sched : SCHED_OTHER,
                        .original_timerslack_ns = proc.timerslack_ns,
                        .original_affinity = orig_aff,
                        .affinity_capped = true,
                        .sched_batch_applied = true,
                        .sched_idle_applied = false,
                        .c2_cluster_dispersed = (is_heavy && topo.cluster_count >= 2),
                        .low_power_ticks = 0
                    });
                    already_tracked = true;
                } else if (already_tracked) {
                    for (auto& tm : m_tracked) {
                        if (tm.pid == proc.pid) {
                            tm.affinity_capped = true;
                            tm.sched_batch_applied = true;
                            if (is_heavy && topo.cluster_count >= 2) tm.c2_cluster_dispersed = true;
                            break;
                        }
                    }
                }
            }
        }
    }

    // Dynamic Variable De-Escalation for C2 dispersed / affinity capped processes (REF-REQ-084)
    for (size_t i = 0; i < m_tracked.size(); ) {
        auto& tm = m_tracked[i];
        if (!tm.c2_cluster_dispersed) {
            ++i;
            continue;
        }

        if (::kill(tm.pid, 0) != 0) {
            m_tracked[i] = m_tracked.back();
            m_tracked.pop_back();
            continue;
        }

        bool found_active = false;
        double cur_watts = 0.0;
        for (const auto& proc : report.top_processes) {
            if (proc.pid == tm.pid) {
                found_active = true;
                cur_watts = proc.cpu_watts;
                break;
            }
        }

        if (!found_active || cur_watts < 0.30) {
            ++tm.low_power_ticks;
        } else {
            tm.low_power_ticks = 0;
        }

        if (tm.low_power_ticks >= 2) {
            restore_core_affinity(tm.pid, &tm.original_affinity);
            restore_sched_normal(tm.pid, tm.original_sched_policy, tm.original_nice);
            core::EventLogger::log_rollback(tm.pid, "heavy-compute", "Adaptive de-escalation: CPU power subsided below 0.3W, restored all-core affinity");
            m_tracked[i] = m_tracked.back();
            m_tracked.pop_back();
        } else {
            ++i;
        }
    }

    // 4. Build active summary string with profile prefix
    const char* profile_label = "[Balanced]";
    if (m_current_profile == PowerProfileMode::PowerSaver) {
        profile_label = "[PowerSaver]";
    } else if (m_current_profile == PowerProfileMode::UltraEndurance) {
        profile_label = "[UltraEndurance]";
    }

    char summary_buf[128];
    if (status.throttled_count == 0 && status.frozen_count == 0 && status.reclaimed_bytes == 0) {
        std::snprintf(summary_buf, sizeof(summary_buf), "%s Optimal (No throttling needed)", profile_label);
    } else {
        std::snprintf(summary_buf, sizeof(summary_buf),
                      "%s %zu throttled, %zu frozen, %luMB reclaimed (~%.2fW saved)",
                      profile_label,
                      status.throttled_count,
                      status.frozen_count,
                      static_cast<unsigned long>(status.reclaimed_bytes / (1024 * 1024)),
                      status.estimated_savings_watts);
    }
    status.active_summary = summary_buf;

    report.mitigation_status = status;
    return status;
}

} // namespace wattcurb::policy
