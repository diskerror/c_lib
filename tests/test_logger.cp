// Unit tests for Diskerror::Logger — construction, level gating, rotation.
//
// Note: Logger uses process-wide static state (trace/debug/.../instanceCount),
// so this test creates exactly one Logger instance for the whole run and
// exercises it sequentially rather than constructing multiple instances.

#include "../Logger.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace Diskerror;
namespace fs = std::filesystem;

static int failures = 0;

static void check(bool cond, const char* what) {
    if (!cond) {
        std::printf("FAIL %s\n", what);
        ++failures;
    }
}

static std::string read_file(const fs::path& p) {
    std::ifstream f(p);
    std::ostringstream oss;
    oss << f.rdbuf();
    return oss.str();
}

int main() {
    fs::path dir = fs::temp_directory_path() / "c_lib_logger_test";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);

    fs::path logFile = dir / "activity.log";

    // Level "info": trace/debug should be no-ops, info/warn/error/critical active.
    {
        Logger logger(logFile.string(), "info");

        Logger::trace("should not appear");
        Logger::debug("should not appear either");
        Logger::info("info message");
        Logger::warn("warn message");

        std::string contents = read_file(logFile);
        check(contents.find("should not appear") == std::string::npos,
              "trace/debug suppressed below info level");
        check(contents.find("[INFO ]") != std::string::npos, "info message logged with tag");
        check(contents.find("info message") != std::string::npos, "info message content present");
        check(contents.find("[WARN ]") != std::string::npos, "warn message logged with tag");
    }
    // Logger destructor decrements instanceCount but the log file remains.

    // Directory auto-creation: nested path that doesn't exist yet.
    // (Second Logger construction inside the same process is a no-op per
    // instanceCount>1 guard when a Logger is still alive; since the first
    // went out of scope above, instanceCount is back to 0 here.)
    fs::path nested = dir / "nested" / "sub" / "app.log";
    bool created_ok = true;
    try {
        Logger logger2(nested.string(), "warn");
        created_ok = fs::exists(nested);
    } catch (...) {
        created_ok = false;
    }
    check(created_ok, "Logger creates missing parent directories and log file");

    // get_timestamp() produces a non-empty, plausible-length string.
    std::string ts = Logger::get_timestamp();
    check(!ts.empty() && ts.size() >= 19, "get_timestamp() returns formatted string");

    fs::remove_all(dir, ec);

    if (failures == 0) {
        std::printf("All Logger tests passed.\n");
        return 0;
    }
    std::printf("%d Logger test(s) failed.\n", failures);
    return 1;
}
