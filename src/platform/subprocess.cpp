// platform/subprocess.cpp — cancellable subprocess execution (task 3.5).
//
// Replaces the original _popen/popen wrapper with CreateProcess (Windows) /
// fork+exec (POSIX) so the caller can kill the child process mid-flight via
// a cancel flag. This is needed so folder switches and program shutdown
// don't block waiting for a slow `git status` to finish.
#include "platform/subprocess.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <errno.h>
#  include <fcntl.h>
#  include <poll.h>
#  include <signal.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif

namespace diffcue::platform::subprocess {

namespace {

std::string shell_quote(const std::string& arg) {
#if defined(_WIN32)
    if (arg.empty() || arg.find_first_of(" \t\"") == std::string::npos) {
        return arg;
    }
    std::string out = "\"";
    for (char c : arg) {
        if (c == '"') out += "\\\"";
        else out += c;
    }
    out += "\"";
    return out;
#else
    if (arg.empty()) return "''";
    if (arg.find_first_of(" \t'\"\\$`") == std::string::npos) return arg;
    std::string out = "'";
    for (char c : arg) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
#endif
}

std::string build_command(const std::string& cmd, const std::vector<std::string>& args) {
    std::string full = shell_quote(cmd);
    for (const auto& a : args) {
        full += " ";
        full += shell_quote(a);
    }
    return full;
}

#if defined(_WIN32)

std::string run_capture_impl(const std::string& cmd,
                             const std::vector<std::string>& args,
                             const std::atomic<bool>* cancel) {
    const std::string full = build_command(cmd, args);
    std::string cmdline = full;

    HANDLE child_out_read = nullptr, child_out_write = nullptr;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;
    if (!CreatePipe(&child_out_read, &child_out_write, &sa, 0)) return {};
    SetHandleInformation(child_out_read, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = child_out_write;
    si.hStdError = child_out_write;
    si.hStdInput = nullptr;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    BOOL ok = CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(child_out_write);
    if (!ok) { CloseHandle(child_out_read); return {}; }

    std::string output;
    char buffer[4096];
    DWORD n_read = 0;

    while (true) {
        if (cancel && cancel->load(std::memory_order_relaxed)) {
            TerminateProcess(pi.hProcess, 1);
            break;
        }
        DWORD available = 0;
        if (!PeekNamedPipe(child_out_read, nullptr, 0, nullptr, &available, nullptr)) break;
        if (available == 0) {
            DWORD exit_code = 0;
            if (GetExitCodeProcess(pi.hProcess, &exit_code) && exit_code != STILL_ACTIVE) break;
            Sleep(10);
            continue;
        }
        DWORD to_read = (available < sizeof(buffer)) ? available : (DWORD)sizeof(buffer);
        if (!ReadFile(child_out_read, buffer, to_read, &n_read, nullptr) || n_read == 0) break;
        output.append(buffer, n_read);
    }

    WaitForSingleObject(pi.hProcess, cancel ? 2000 : INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(child_out_read);
    if (exit_code != 0) return {};
    return output;
}

#else

std::string run_capture_impl(const std::string& cmd,
                             const std::vector<std::string>& args,
                             const std::atomic<bool>* cancel) {
    int pipefd[2];
    if (pipe(pipefd) != 0) return {};

    pid_t pid = fork();
    if (pid < 0) { close(pipefd[0]); close(pipefd[1]); return {}; }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        std::vector<const char*> argv;
        argv.reserve(args.size() + 2);
        argv.push_back(cmd.c_str());
        for (const auto& a : args) argv.push_back(a.c_str());
        argv.push_back(nullptr);
        execvp(cmd.c_str(), const_cast<char* const*>(argv.data()));
        _exit(127);
    }

    close(pipefd[1]);
    std::string output;
    char buffer[4096];

    while (true) {
        if (cancel && cancel->load(std::memory_order_relaxed)) {
            kill(pid, SIGTERM);
            break;
        }
        struct pollfd pfd;
        pfd.fd = pipefd[0];
        pfd.events = POLLIN;
        pfd.revents = 0;
        int rc = poll(&pfd, 1, 100);
        if (rc < 0) { if (errno == EINTR) continue; break; }
        if (rc == 0) continue;
        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            ssize_t n;
            while ((n = read(pipefd[0], buffer, sizeof(buffer))) > 0) output.append(buffer, n);
            break;
        }
        if (pfd.revents & POLLIN) {
            ssize_t n = read(pipefd[0], buffer, sizeof(buffer));
            if (n <= 0) break;
            output.append(buffer, n);
        }
    }

    close(pipefd[0]);
    int status = 0;
    if (cancel) {
        for (int i = 0; i < 50; ++i) {
            if (waitpid(pid, &status, WNOHANG) == pid) goto done;
            usleep(10000);
        }
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
    } else {
        waitpid(pid, &status, 0);
    }
done:
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return {};
    return output;
}

#endif

}  // namespace

std::string run_capture(const std::string& cmd, const std::vector<std::string>& args,
                        const std::atomic<bool>* cancel) {
    return run_capture_impl(cmd, args, cancel);
}

bool run_succeeds(const std::string& cmd, const std::vector<std::string>& args) {
    return !run_capture(cmd, args).empty();
}

}  // namespace diffcue::platform::subprocess
