#ifdef PS2_PLATFORM

#include "ps2/network/Ps2Network.h"

#include "platform/Log.h"
#include "platform/Mutex.h"
#include "ps2/system/Ps2IrxLoader.h"

#include <delaythread.h>
#include <kernel.h>
#include <loadfile.h>

extern "C" {
#ifdef PS2_REMOTE_DEBUG
#include <ps2ips.h>
#include <ps2ip_rpc.h>

// libps2ips keeps this low-level configuration RPC internal instead of
// exposing it through ps2ips.h. Read the ps2link-owned sm0 address so client
// sockets can bind to the existing interface without replacing its config.
int ps2ipc_ps2ip_getconfig(char *netif_name, t_ip_info *ip_info);
#else
#include <ps2ip.h>
#include <netman.h>
extern unsigned char ps2dev9_irx[];
extern unsigned int size_ps2dev9_irx;
extern unsigned char netman_irx[];
extern unsigned int size_netman_irx;
extern unsigned char smap_irx[];
extern unsigned int size_smap_irx;
#endif
}

#include <atomic>
#include <mutex>

namespace
{
PlatformMutex s_initMutex;
std::atomic_bool s_ready{false};
bool s_stackInitialized = false;
bool s_dhcpStarted = false;
bool s_linkModeConfigured = false;
std::uint32_t s_localAddressNetworkOrder = 0;

#if defined(PS2_ENABLE_NETWORK) && !defined(PS2_REMOTE_DEBUG)
bool loadEmbeddedNetworkModule(const char* name, unsigned char* data, unsigned int size)
{
    int moduleResult = 1;
    const int result = SifExecModuleBuffer(data, static_cast<int>(size), 0, nullptr, &moduleResult);
    MC_LOG_INFO("network", "[PS2] embedded %s -> load=%d start=%d\n",
                name, result, moduleResult);
    return result >= 0 && moduleResult >= 0 && moduleResult != 1;
}

bool loadNetworkModules()
{
    // The EE-side ps2ip stack needs the Ethernet driver, NETMAN bridge, and
    // SMAP Ethernet driver on the IOP. Load these from the ELF so network
    // availability does not depend on optional data/irx files on USB/MX4SIO.
    // A launcher may already have some of these modules resident. Duplicate
    // module loads can fail even though the existing Ethernet path is usable;
    // ps2ipInit() and the sm0 registration check below determine readiness.
    (void)loadEmbeddedNetworkModule("ps2dev9", ps2dev9_irx, size_ps2dev9_irx);
    (void)loadEmbeddedNetworkModule("netman", netman_irx, size_netman_irx);
    (void)loadEmbeddedNetworkModule("smap", smap_irx, size_smap_irx);
    return true;
}
#endif

bool startNetworkStack()
{
#ifdef NO_NETWORK
    return false;
#else
    if (s_stackInitialized)
        return true;

#ifdef PS2_REMOTE_DEBUG
    // ps2link already owns DEV9/SMAP and PS2IP-NM, but it does not load the
    // ps2ips RPC socket server. Load only that bridge module; reloading the
    // hardware/network stack would compete with ps2link and break remote I/O.
    MC_LOG_INFO("network", "[PS2] loading ps2ips RPC bridge for ps2link stack\n");
    McLog::flush();
    const int ps2ips = Ps2IrxLoader::load("irx/ps2ips.irx", "host:ps2ips.irx");
    if (ps2ips < 0)
    {
        // ps2ip_init() retries SIF RPC binding forever when no PS2IPS server is
        // present. Fail here instead of freezing the multiplayer worker/UI.
        MC_LOG_ERROR("network", "[PS2] ps2ips.irx unavailable; refusing blocking RPC bind\n");
        return false;
    }

    MC_LOG_INFO("network", "[PS2] ps2ips RPC bridge loaded id=%d; binding EE client\n", ps2ips);
    McLog::flush();
    if (ps2ip_init() < 0)
    {
        MC_LOG_ERROR("network", "[PS2] ps2ips RPC initialization failed\n");
        return false;
    }

    s_stackInitialized = true;
    MC_LOG_INFO("network", "[PS2] ps2ips RPC client ready; preserving ps2link network config\n");

    // Keep the exact network-order address returned by PS2IP-NM. Reusing the
    // binary value avoids reparsing it through the incompatible EE inet_aton()
    // path before the explicit client bind.
    t_ip_info info{};
    if (ps2ipc_ps2ip_getconfig(const_cast<char *>("sm0"), &info) > 0)
        s_localAddressNetworkOrder = info.ipaddr.s_addr;
    else
    {
        MC_LOG_WARN("network", "[PS2] failed to read ps2link sm0 configuration through ps2ips\n");
    }

    McLog::flush();
    return true;
#elif defined(PS2_ENABLE_NETWORK)
    if (!loadNetworkModules())
    {
        MC_LOG_ERROR("network", "[PS2] failed to load embedded Ethernet IOP modules\n");
        return false;
    }

    // ps2ipInit initializes NETMAN itself, then registers the EE-side lwIP
    // stack as sm0. Calling NetManInit a second time here is unnecessary and
    // can make ownership/deinit semantics ambiguous.
    ip4_addr ip{};
    ip4_addr mask{};
    ip4_addr gateway{};
    IP4_ADDR(&ip, 169, 254, 0, 1);
    IP4_ADDR(&mask, 255, 255, 0, 0);
    IP4_ADDR(&gateway, 0, 0, 0, 0);
    MC_LOG_INFO("network", "[PS2] calling ps2ipInit\n");
    McLog::flush();
    if (ps2ipInit(&ip, &mask, &gateway) < 0)
    {
        MC_LOG_ERROR("network", "[PS2] ps2ipInit failed\n");
        return false;
    }

    MC_LOG_INFO("network", "[PS2] ps2ipInit OK\n");
    McLog::flush();

    t_ip_info info{};
    if (!ps2ip_getconfig(const_cast<char *>("sm0"), &info))
    {
        MC_LOG_ERROR("network", "[PS2] network interface sm0 was not registered\n");
        ps2ipDeinit();
        return false;
    }

    s_stackInitialized = true;
    MC_LOG_INFO("network", "[PS2] sm0 registered\n");
    McLog::flush();
    return true;
#else
    return false;
#endif
#endif
}

#if defined(PS2_ENABLE_NETWORK) && !defined(PS2_REMOTE_DEBUG)
bool configureEthernetAutoNegotiation()
{
    if (s_linkModeConfigured)
        return true;

    const int result = NetManSetLinkMode(NETMAN_NETIF_ETH_LINK_MODE_AUTO);
    if (result != 0)
    {
        MC_LOG_ERROR("network", "[PS2] failed to enable Ethernet auto-negotiation: %d\n", result);
        return false;
    }

    s_linkModeConfigured = true;
    MC_LOG_INFO("network", "[PS2] Ethernet auto-negotiation enabled\n");
    return true;
}

bool waitForEthernetLink()
{
    constexpr int kLinkWaitAttempts = 20;
    constexpr int kLinkPollIntervalUs = 250000;
    for (int attempt = 0; attempt < kLinkWaitAttempts; ++attempt)
    {
        if (NetManIoctl(NETMAN_NETIF_IOCTL_GET_LINK_STATUS, nullptr, 0, nullptr, 0) ==
            NETMAN_NETIF_ETH_LINK_STATE_UP)
        {
            MC_LOG_INFO("network", "[PS2] Ethernet link is up\n");
            return true;
        }

        if (attempt + 1 < kLinkWaitAttempts)
            DelayThread(kLinkPollIntervalUs);
    }

    MC_LOG_ERROR("network", "[PS2] Ethernet link did not come up; check the cable and adapter\n");
    return false;
}

bool startDhcp()
{
    if (s_dhcpStarted)
        return true;

    t_ip_info info{};
    if (!ps2ip_getconfig(const_cast<char *>("sm0"), &info))
    {
        MC_LOG_ERROR("network", "[PS2] cannot configure DHCP: sm0 is unavailable\n");
        return false;
    }

    info.dhcp_enabled = 1;
    if (!ps2ip_setconfig(&info))
    {
        MC_LOG_ERROR("network", "[PS2] failed to enable DHCP on sm0\n");
        return false;
    }

    s_dhcpStarted = true;
    MC_LOG_INFO("network", "[PS2] DHCP client started after Ethernet link became active\n");
    return true;
}

bool hasDhcpLease()
{
    t_ip_info info{};
    if (!ps2ip_getconfig(const_cast<char *>("sm0"), &info))
        return false;
    return info.dhcp_enabled && info.dhcp_status == DHCP_STATE_BOUND;
}
#endif
}

namespace Ps2Network
{
bool initialize()
{
#ifdef NO_NETWORK
    return false;
#else
    if (s_ready.load(std::memory_order_acquire))
        return true;

    MC_LOG_INFO("network", "[PS2] network initialize entered\n");
    McLog::flush();
    std::lock_guard<PlatformMutex> guard(s_initMutex);
    MC_LOG_INFO("network", "[PS2] network init lock acquired\n");
    McLog::flush();
    if (s_ready.load(std::memory_order_relaxed))
        return true;
    if (!startNetworkStack())
        return false;

#ifndef PS2_REMOTE_DEBUG
    // Wait for PHY auto-negotiation before starting DHCP; otherwise the first
    // client can send its DHCP discovery while SMAP still reports no carrier.
    if (!configureEthernetAutoNegotiation() || !waitForEthernetLink() || !startDhcp())
        return false;

    // DHCP runs asynchronously. Check at a coarse interval instead of adding a
    // fixed grace-period sleep; connections proceed as soon as the lease exists.
    constexpr int kDhcpPollIntervalUs = 2000000;
    constexpr int kDhcpMaxPolls = 5;
    bool leaseReady = hasDhcpLease();
    for (int poll = 0; !leaseReady && poll < kDhcpMaxPolls; ++poll)
    {
        DelayThread(kDhcpPollIntervalUs);
        leaseReady = hasDhcpLease();
    }
    if (!leaseReady)
    {
        MC_LOG_ERROR("network", "[PS2] DHCP has not acquired a lease yet\n");
        return false;
    }
#endif

    s_ready.store(true, std::memory_order_release);
#ifdef PS2_REMOTE_DEBUG
    MC_LOG_INFO("network", "[PS2] ps2link shared network ready; enabling socket attempts\n");
#else
    MC_LOG_INFO("network", "[PS2] DHCP lease acquired; enabling socket attempts\n");
#endif
    return true;
#endif
}

bool isReady()
{
    return s_ready.load(std::memory_order_acquire);
}

bool localAddressNetworkOrder(std::uint32_t &address)
{
    std::lock_guard<PlatformMutex> guard(s_initMutex);
    if (s_localAddressNetworkOrder == 0)
        return false;
    address = s_localAddressNetworkOrder;
    return true;
}
}

#endif
