// Message queues vs. pipes: a pipe is a raw, unstructured byte stream
// -- the reader has to already agree on how to split it into messages
// (fixed size, delimiter, length prefix...). A POSIX message queue
// delivers discrete messages: each mq_send() is received whole by
// exactly one mq_receive(), and -- the distinguishing feature this
// demo is built to show -- each message carries a priority, and
// mq_receive() always returns the highest-priority message currently
// queued, regardless of what order messages were sent in.
//
// Named message queues are also kernel-persistent independent of any
// process: unlike a pipe (which disappears once every fd referencing
// it is closed), a queue created with mq_open(..., O_CREAT, ...)
// exists in the kernel under its name until something calls
// mq_unlink() on it -- even if every process that touched it has
// exited. Forgetting to unlink leaks it until reboot.
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <mqueue.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

static const char* kQueueName = "/os_components_ipc_demo";

static void send_message(mqd_t mq, const char* text, unsigned priority) {
    if (mq_send(mq, text, std::strlen(text), priority) == -1) {
        perror("mq_send");
        _exit(1);
    }
}

int main() {
    // Defensive cleanup: if a previous run crashed before mq_unlink(),
    // the named queue would still exist in the kernel. Ignore ENOENT
    // (the common case: it doesn't exist yet).
    mq_unlink(kQueueName);

    struct mq_attr attr = {};
    attr.mq_flags   = 0;
    attr.mq_maxmsg  = 10;
    attr.mq_msgsize = 256;
    attr.mq_curmsgs = 0;

    mqd_t mq = mq_open(kQueueName, O_CREAT | O_RDWR, 0644, &attr);
    if (mq == static_cast<mqd_t>(-1)) {
        perror("mq_open");
        return 1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        // Child: sends three messages with priorities deliberately out
        // of order, to prove the queue reorders by priority rather
        // than preserving send order like a pipe would.
        send_message(mq, "low priority message", 1);
        send_message(mq, "high priority message", 10);
        send_message(mq, "medium priority message", 5);
        mq_close(mq);
        _exit(0);
    }

    // Parent: wait for the child to finish sending *before* receiving
    // anything. This is what makes the reordering visible -- the
    // messages sit in the kernel queue independent of the child still
    // being alive, so all three are available for mq_receive() to pick
    // the highest priority among, rather than being handed out one at
    // a time as they arrive.
    waitpid(pid, nullptr, 0);

    char buf[256];
    unsigned priority = 0;
    std::string received[3];

    for (int i = 0; i < 3; ++i) {
        ssize_t n = mq_receive(mq, buf, sizeof(buf), &priority);
        assert(n >= 0);
        received[i].assign(buf, static_cast<std::size_t>(n));
        std::printf("Received (priority %u): \"%s\"\n", priority, received[i].c_str());
    }

    mq_close(mq);
    mq_unlink(kQueueName);

    assert(received[0] == "high priority message");
    assert(received[1] == "medium priority message");
    assert(received[2] == "low priority message");

    std::printf("All tests passed.\n");
}
