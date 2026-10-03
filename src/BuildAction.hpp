#pragma once
#include "PhysicsEngine.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <string>
#include <stdexcept>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

// Build in a child process so the playground stays responsive.
class BuildAction {
public:
    std::string status;
    void poll() {
        if (child <= 0) return;
        int result = 0;
        if (waitpid(child, &result, WNOHANG) == child) {
            child = -1;
            status = WIFEXITED(result) && WEXITSTATUS(result) == 0
                ? "OK: compiled, tested, daemon active"
                : "Build failed: see build/playground-build.log";
        }
    }
    void start(const PhysicsEngine& physics) {
        if (child > 0) return;
        namespace fs = std::filesystem;
        try {
            auto root = fs::current_path();
            if (!fs::is_regular_file(root / "build.sh")) {
                root = fs::read_symlink("/proc/self/exe").parent_path();
                if (!fs::is_regular_file(root / "build.sh")) root = root.parent_path();
            }
            if (!fs::is_regular_file(root / "build.sh") || !fs::is_directory(root / "src")) {
                status = "Open the playground from the source checkout";
                return;
            }
            const auto dest = root / "src/PhysicsDefaults.hpp";
            const auto temp = root / "src/PhysicsDefaults.hpp.tmp";
            std::ofstream out(temp);
            out.exceptions(std::ios::failbit | std::ios::badbit);
            out << "#pragma once\n\n// Saved by F9 in the playground.\nnamespace PhysicsDefaults {\n";
            out << std::scientific << std::setprecision(std::numeric_limits<float>::max_digits10);
            const auto write = [&](const char* name, float value) {
                out << "inline constexpr float " << name << " = " << value << "f;\n";
            };
            write("mass", physics.mass);
            write("springK", physics.springK);
            write("damping", physics.damping);
            write("velocityInfluence", physics.velocityInfluence);
            write("inertiaInfluence", physics.inertiaInfluence);
            write("maxDeflectionDeg", physics.maxDeflectionDeg);
            out << "}\n";
            out.close();
            const int saved = open(temp.c_str(), O_RDONLY);
            if (saved < 0) throw std::runtime_error("Cannot sync saved parameters");
            const int synced = fsync(saved);
            close(saved);
            if (synced != 0) throw std::runtime_error("Cannot sync saved parameters");
            fs::rename(temp, dest);
            fs::create_directories(root / "build");
            const auto script = (root / "build.sh").string();
            const auto log = (root / "build/playground-build.log").string();
            child = fork();
            if (child == 0) {
                const int fd = open(log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
                if (fd < 0) _exit(126);
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                close(fd);
                execl("/bin/bash", "bash", script.c_str(), "--daemon-only", "--apply", nullptr);
                _exit(127);
            }
            status = child < 0 ? "Could not start compiler" : "Compiling and testing...";
        } catch (const std::exception& error) {
            status = std::string("Build: ") + error.what();
        }
    }
private:
    pid_t child = -1;
};
