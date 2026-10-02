#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

/* The high_speed_platformer example is meant to be lifted into its own repository (plan 29.2), so it
 * must lean on the public library only. This test reads the example's sources and build rules and
 * fails on anything that would not survive the move: an include of another example's or of the
 * library's private files, an example-only helper, or a build rule that reaches into a sibling. */

namespace fs = std::filesystem;

static int failures = 0;

static void fail(const std::string& file, const std::string& what) {
    std::fprintf(stderr, "FAIL %s: %s\n", file.c_str(), what.c_str());
    ++failures;
}

static std::string slurp(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static bool contains(const std::string& text, const char* needle) {
    return text.find(needle) != std::string::npos;
}

static void check_source(const fs::path& path) {
    const std::string name = path.generic_string();
    std::istringstream lines(slurp(path));
    std::string line;
    while (std::getline(lines, line)) {
        const size_t hash = line.find_first_not_of(" \t");
        if (hash == std::string::npos || line.compare(hash, 8, "#include") != 0) continue;
        const size_t open = line.find_first_of("\"<", hash);
        if (open == std::string::npos) continue;
        const char closer = line[open] == '"' ? '"' : '>';
        const size_t close = line.find(closer, open + 1);
        if (close == std::string::npos) continue;
        const std::string inc = line.substr(open + 1, close - open - 1);
        if (inc.rfind("saturn/", 0) == 0) {
            if (!fs::exists(fs::path("include") / inc)) fail(name, "includes a header that is not public: " + inc);
        } else if (inc.find("..") != std::string::npos || inc.find("example_util") != std::string::npos ||
                   inc.rfind("common/", 0) == 0 || inc.rfind("src/", 0) == 0) {
            fail(name, "forbidden include: " + inc);
        } else if (inc.find('/') != std::string::npos && inc.rfind("high_speed_platformer/", 0) != 0) {
            fail(name, "includes something outside the example: " + inc);
        }
    }
    if (contains(slurp(path), "sat_example_must")) fail(name, "uses the shared example macro; define a local one");
}

static void check_build_file(const fs::path& path) {
    const std::string name = path.generic_string();
    std::istringstream lines(slurp(path));
    std::string line;
    while (std::getline(lines, line)) {
        size_t at = 0;
        while ((at = line.find("examples/", at)) != std::string::npos) {
            const bool own = line.compare(at, 31, "examples/high_speed_platformer/") == 0 ||
                             line.compare(at, 30, "examples/high_speed_platformer") == 0 ||
                             line.compare(at, 15, "examples/*/host") == 0;
            if (!own && !(line.size() > 0 && line[0] == '#')) fail(name, "refers to another example: " + line);
            at += 9;
        }
        if (contains(line, "examples/common")) fail(name, "refers to examples/common: " + line);
    }
}

int main() {
    const fs::path dir = "examples/high_speed_platformer";
    if (!fs::is_directory(dir)) {
        fail(dir.generic_string(), "run the test from the repository root");
        return 1;
    }
    size_t sources = 0;
    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        const std::string ext = entry.path().extension().string();
        if (ext == ".c" || ext == ".h") {
            check_source(entry.path());
            ++sources;
        } else if (ext == ".mk" || ext == ".inc") {
            check_build_file(entry.path());
        } else if (ext == ".py") {
            const std::string text = slurp(entry.path());
            if (contains(text, "examples/") && !contains(text, "examples/high_speed_platformer")) {
                fail(entry.path().generic_string(), "refers to another example");
            }
        }
    }
    if (sources < 6) fail(dir.generic_string(), "expected the example's sources, found fewer than six");
    for (const char* needed : {"Makefile.inc", "stage.mk", "host_test.mk", "README.md", "tools/gen_stage.py", "main.c", "game.c"}) {
        if (!fs::exists(dir / needed)) fail((dir / needed).generic_string(), "missing");
    }
    if (failures != 0) return 1;
    std::printf("high_speed_platformer boundary: %zu sources clean\n", sources);
    return 0;
}
