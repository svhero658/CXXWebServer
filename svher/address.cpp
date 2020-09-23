#include <memory>
#include <sstream>
#include <string>
#include <netinet/in.h>
#include "address.h"
#include "endian.h"
#include "log.h"

namespace svher {

    template<class T>
    static T CreateMask(uint32_t bits) {
        return (1 << (sizeof(T) * 8 - bits)) - 1;
    }

    static Logger::ptr g_logger = LOG_NAME("sys");

    int Address::getFamily() const {
        return getAddr()->sa_family;
    }

    std::string Address::toString() const {
        std::stringstream ss;
        insert(ss);
        return ss.str();
    }

    bool Address::operator<(const Address &rhs) const {
        socklen_t minLength = std::min(getAddrLen(), rhs.getAddrLen());
        int ret = memcmp(getAddr(), rhs.getAddr(), minLength);
        if (ret < 0) return true;
        else if (ret > 0) return false;
        else return getAddrLen() < rhs.getAddrLen();
    }

    bool Address::operator==(const Address &rhs) const {
        return getAddrLen() == rhs.getAddrLen()
                && memcmp(getAddr(), rhs.getAddr(), getAddrLen()) == 0;
    }

    bool Address::operator!=(const Address &rhs) const {
        // 利用上面的函数
        return !(*this == rhs);
    }

    IPv4Address::IPv4Address(uint32_t address, uint32_t port) {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sin_family = AF_INET;
        m_addr.sin_port = byteswapToBigEndian(port);
        m_addr.sin_addr.s_addr = byteswapToBigEndian(address);
    }

    const sockaddr *IPv4Address::getAddr() const {
        return (sockaddr*)&m_addr;
    }

    socklen_t IPv4Address::getAddrLen() const {
        return sizeof(sockaddr);
    }

    std::ostream &IPv4Address::insert(std::ostream &os) const {
        uint32_t addr = byteswaptoLittleEndian(m_addr.sin_addr.s_addr);
        os << ((addr >> 24) & 0xff) << "."
            << ((addr >> 16) & 0xff) << "."
            << ((addr >> 8) & 0xff) << "."
            << (addr & 0xff);
        os << ":" << byteswaptoLittleEndian(m_addr.sin_port);
        return os;
    }

    IPAddress::ptr IPv4Address::broadcastAddress(uint32_t prefix_len) {
        if (prefix_len > 32) return nullptr;
        sockaddr_in baddr(m_addr);
        baddr.sin_addr.s_addr |= byteswaptoLittleEndian(
                CreateMask<uint32_t>(prefix_len));
        return std::make_shared<IPv4Address>(baddr);
    }

    IPAddress::ptr IPv4Address::networkAddress(uint32_t prefix_len) {
        if (prefix_len > 32) return nullptr;
        sockaddr_in baddr(m_addr);
        baddr.sin_addr.s_addr &= byteswaptoLittleEndian(
                CreateMask<uint32_t>(prefix_len));
        return std::make_shared<IPv4Address>(baddr);
    }

    IPAddress::ptr IPv4Address::subnetMask(uint32_t prefix_len) {
        sockaddr_in saddr;
        memset(&saddr, 0, sizeof(saddr));
        saddr.sin_family = AF_INET;
        saddr.sin_addr.s_addr = ~byteswaptoLittleEndian(CreateMask<uint32_t>(prefix_len));
        return std::make_shared<IPv4Address>(saddr);
    }

    uint32_t IPv4Address::getPort() const {
        return byteswaptoLittleEndian(m_addr.sin_port);
    }

    void IPv4Address::setPort(uint32_t v) {
        m_addr.sin_port = byteswapToBigEndian(v);
    }

    IPv6Address::IPv6Address(const char *address, uint32_t port) {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sin6_family = AF_INET6;
        m_addr.sin6_port = byteswapToBigEndian(port);
        memcpy(&m_addr.sin6_addr.s6_addr, address, 16);
    }

    const sockaddr *IPv6Address::getAddr() const {
        return (sockaddr*)&m_addr;
    }

    socklen_t IPv6Address::getAddrLen() const {
        return sizeof(m_addr);
    }

    std::ostream &IPv6Address::insert(std::ostream &os) const {
        os << "[";
        auto* addr = (uint16_t*)m_addr.sin6_addr.s6_addr;
        bool used_zeros = false;
        for (int i = 0; i < 16; ++i) {
            // 当 i > 0 时，输出 addr[i]，若遇到 0 跳过
            if (addr[i] == 0 && !used_zeros) {
                continue;
            }
            // 如果当前不为 0，前一个块为 0，则补输出一个前导分隔符
            if (i && addr[i - 1] == 0 && !used_zeros) {
                os << ":";
                used_zeros = true;
            }
            if (i) {
                os << ":";
            }
            os << std::hex << (int)(byteswaptoLittleEndian(addr[i])) << std::dec;
        }
        if (!used_zeros && addr[7] == 0) {
            os << "::";
        }
        os << "]:" << byteswaptoLittleEndian(m_addr.sin6_port);
        return os;
    }

    IPAddress::ptr IPv6Address::broadcastAddress(uint32_t prefix_len) {
        sockaddr_in6 baddr(m_addr);
        baddr.sin6_addr.s6_addr[prefix_len / 8] |= CreateMask<uint32_t>(prefix_len % 8);
        for (int i = prefix_len / 8 + 1; i < 16; ++i) {
            baddr.sin6_addr.s6_addr[i] = 0xff;
        }
    }

    IPAddress::ptr IPv6Address::networkAddress(uint32_t prefix_len) {
        return svher::IPAddress::ptr();
    }

    IPAddress::ptr IPv6Address::subnetMask(uint32_t prefix_len) {
        return svher::IPAddress::ptr();
    }

    uint32_t IPv6Address::getPort() const {
        return byteswaptoLittleEndian(m_addr.sin6_port);
    }

    void IPv6Address::setPort(uint32_t v) {
        m_addr.sin6_port = byteswapToBigEndian(v);
    }

    // -1 因为 0 地址已经占用了 1 空间
    const static size_t MAX_PATH_LEN = sizeof(((sockaddr_un*)nullptr)->sun_path) - 1;

    UnixAddress::UnixAddress() {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sun_family = AF_UNIX;
        m_length = offsetof(sockaddr_un, sun_path) + MAX_PATH_LEN;
    }

    UnixAddress::UnixAddress(const std::string &path) {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sun_family = AF_UNIX;
        m_length = path.size() + 1;
        if (!path.empty() && path[0] == '\0') {
            --m_length;
        }
        if (m_length <= sizeof(m_addr.sun_path)) {
            throw std::logic_error("path too long");
        }
        memcpy(&m_addr.sun_path, path.c_str(), m_length);
        m_length += offsetof(sockaddr_un, sun_path);
    }

    const sockaddr *UnixAddress::getAddr() const {
        return (sockaddr*)&m_addr;
    }

    socklen_t UnixAddress::getAddrLen() const {
        return m_length;
    }

    std::ostream &UnixAddress::insert(std::ostream &os) const {
        if (m_length > offsetof(sockaddr_un, sun_path)
                && m_addr.sun_path[0] == '\0') {
            return os << "\\0" << std::string(m_addr.sun_path + 1,
                              m_length - offsetof(sockaddr_un, sun_path) - 1);
        }
        return os << m_addr.sun_path;
    }


    UnknownAddress::UnknownAddress(int family) {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sa_family = family;
    }

    const sockaddr *UnknownAddress::getAddr() const {
        return &m_addr;
    }

    socklen_t UnknownAddress::getAddrLen() const {
        return sizeof(m_addr);
    }

    std::ostream &UnknownAddress::insert(std::ostream &os) const {
        os << "[UnknownAddress family=" << m_addr.sa_family << "]";
        return os;
    }
}