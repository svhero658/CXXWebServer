#include <execinfo.h>
#include <ctime>
#include <sys/time.h>
#include "util.h"
#include "fiber.h"
#include "macro.h"
#include "log.h"

namespace svher {
    static Logger::ptr g_logger = LOG_NAME("sys");

    pid_t GetThreadId() {
        return syscall(SYS_gettid);
    }

    uint32_t GetFiberId() {
        return Fiber::GetFiberId();
    }

    void Backtrace(std::vector<std::string> &bt, int size, int skip) {
        void** array = (void**)malloc((sizeof(void*) * size));
        size_t s = ::backtrace(array, size);
        char** strings = backtrace_symbols(array, s);
        if (strings == nullptr) {
            LOG_ERROR(g_logger) << "backtrack_symbols error";
            return;
        }
        for (size_t i = skip; i < s; ++i) {
            bt.emplace_back(strings[i]);
        }
        free(strings);
        free(array);
    }

    std::string BacktraceToString(int size, const std::string& prefix, int skip) {
        std::vector<std::string> bt;
        Backtrace(bt, size, skip);
        std::stringstream ss;
        for (auto & i : bt) {
            ss << prefix << i << std::endl;
        }
        return ss.str();
    }

    uint64_t GetCurrentMS() {
        timeval tv;
        int ret = gettimeofday(&tv, nullptr);
        ASSERT(!ret);
        return tv.tv_sec * 1000ul + tv.tv_usec / 1000;
    }

    uint64_t GetCurrentUS() {
        timeval tv;
        gettimeofday(&tv, nullptr);
        return tv.tv_sec * 1000 * 1000ul + tv.tv_usec;
    }

}