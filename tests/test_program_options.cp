// Unit tests for Diskerror::ProgramOptions — thin wrapper over boost::program_options.

#include "../ProgramOptions.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace Diskerror;

static int failures = 0;

static void check(bool cond, const char* what) {
    if (!cond) {
        std::printf("FAIL %s\n", what);
        ++failures;
    }
}

// Build an argv-style array from string literals for run(argc, argv).
struct Args {
    std::vector<std::string> storage;
    std::vector<char*> argv;

    explicit Args(std::vector<std::string> vals) : storage(std::move(vals)) {
        for (auto& s : storage) argv.push_back(s.data());
    }
    int argc() const { return static_cast<int>(argv.size()); }
    char** data() { return argv.data(); }
};

int main() {
    // Basic flag + value parsing
    {
        ProgramOptions opts("test options");
        opts.add_options()
            ("verbose,v", po::bool_switch(), "enable verbose output")
            ("name,n", po::value<std::string>(), "a name");

        Args args({"prog", "--verbose", "--name", "reid"});
        opts.run(args.argc(), args.data());

        check(opts.count("verbose") == 1, "bool switch flag counted");
        check(opts["verbose"].as<bool>() == true, "bool switch value true");
        check(opts.count("name") == 1, "value option counted");
        check(opts["name"].as<std::string>() == "reid", "value option content");
    }

    // Positional arguments
    {
        ProgramOptions opts("positional test");
        opts.add_options()
            ("input", po::value<std::vector<std::string>>(), "input files");
        opts.add_positional("input", -1);

        Args args({"prog", "file1.wav", "file2.wav"});
        opts.run(args.argc(), args.data());

        auto params = opts.getParams("input");
        check(params.size() == 2, "positional args captured as multiple values");
        check(params.size() == 2 && params[0] == "file1.wav" && params[1] == "file2.wav",
              "positional args in order");
    }

    // getParams() on missing key returns empty vector, not a throw
    {
        ProgramOptions opts("missing key test");
        opts.add_options()("foo", po::value<std::vector<std::string>>(), "foo");
        Args args({"prog"});
        opts.run(args.argc(), args.data());

        auto params = opts.getParams("foo");
        check(params.empty(), "getParams() on unset key returns empty vector");
        check(opts.count("foo") == 0, "count() is 0 for unset key");
    }

    // Hidden options are parsed but not shown in to_string() output
    {
        ProgramOptions opts("hidden test");
        opts.add_options()("shown", po::value<std::string>(), "a visible option");
        opts.add_hidden_options()("secret", po::value<std::string>(), "a hidden option");

        Args args({"prog", "--shown", "yes", "--secret", "hush"});
        opts.run(args.argc(), args.data());

        check(opts.count("secret") == 1, "hidden option still parsed");
        std::string help = opts.to_string();
        check(help.find("shown") != std::string::npos, "visible option appears in to_string()");
        check(help.find("secret") == std::string::npos, "hidden option absent from to_string()");
    }

    if (failures == 0) {
        std::printf("All ProgramOptions tests passed.\n");
        return 0;
    }
    std::printf("%d ProgramOptions test(s) failed.\n", failures);
    return 1;
}
