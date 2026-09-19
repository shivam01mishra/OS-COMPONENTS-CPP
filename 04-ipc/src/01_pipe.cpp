// Pipes: a kernel-buffered, unidirectional byte stream between related
// processes. pipe() hands back two file descriptors -- fds[0] (read
// end) and fds[1] (write end) -- that both refer to the same kernel
// buffer. fork() then duplicates the calling process's entire fd
// table, so both parent and child end up holding copies of BOTH ends.
//
// Convention here: child writes, parent reads. Each process closes the
// end it doesn't use. This isn't just tidiness -- it's required for
// correctness: read() only sees EOF once every open copy of the write
// end (across every process) has been closed. If the parent forgot to
// close its own copy of fds[1], its read() would block forever even
// after the child finished writing and exited, because the kernel
// would still see a write end open (the parent's own unused copy).
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

int main() {
    int fds[2];
    if (pipe(fds) == -1) {
        perror("pipe");
        return 1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        // Child: writer. Close the read end -- we never use it, and
        // leaving it open would (harmlessly here, but in general)
        // count as another reader reference the kernel has to track.
        close(fds[0]);

        std::string msg = "Hello from child (pid=" + std::to_string(getpid()) + ")";
        write(fds[1], msg.c_str(), msg.size());

        // Closing the write end is what lets the parent's read() see
        // EOF after this message -- without it, the parent would block
        // waiting for more data that will never come.
        close(fds[1]);
        _exit(0);
    }

    // Parent: reader. Close the write end -- if we didn't, our own
    // unused copy would keep the pipe "open for writing" forever from
    // the kernel's point of view, even after the child closes its copy.
    close(fds[1]);

    char buf[256] = {};
    // A single read() is not guaranteed to return the whole message for
    // arbitrary sizes -- the kernel is free to hand back fewer bytes
    // than requested. It's safe here only because this message is much
    // smaller than the pipe's kernel buffer (typically 64 KB on Linux);
    // a general-purpose reader would loop until it has everything it
    // expects or read() returns 0 (EOF).
    ssize_t n = read(fds[0], buf, sizeof(buf) - 1);
    close(fds[0]);

    // Reap the child so it doesn't linger as a zombie process entry --
    // the kernel keeps a terminated child's exit status around until
    // the parent collects it with wait()/waitpid().
    int status = 0;
    waitpid(pid, &status, 0);

    std::printf("Parent (pid=%d) received: \"%s\"\n", getpid(), buf);

    std::string expected_prefix = "Hello from child (pid=";
    assert(n > 0);
    assert(std::string(buf, static_cast<std::size_t>(n)).rfind(expected_prefix, 0) == 0);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);

    std::printf("All tests passed.\n");
}
