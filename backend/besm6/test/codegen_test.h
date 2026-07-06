#pragma once

#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "besm.h"
#include "codegen.h"
#include "parser.h"
#include "semantic.h"
#include "structtab.h"
#include "symtab.h"
#include "tac.h"
#include "target.h"
#include "test_preprocess.h"
#include "translate.h"
#include "typetab.h"
#include "xalloc.h"

extern "C" int xalloc_debug;

class CodegenTest : public ::testing::Test {
    FILE *input_file{};
    Program *program{};
    OptFlags opt_flags{};

    // RAII advisory lock used to detect a second concurrent besm-tests run of the same
    // test (which would clobber the shared <TestName>.dub/.lst).  Non-blocking: if another
    // process already holds it, locked() is false and the caller fails fast.  The kernel
    // releases the lock on close()/process exit, so a crashed run never leaves it stuck.
    class FlockGuard {
    public:
        explicit FlockGuard(const std::string &path)
            : fd_(open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0644))
        {
            if (fd_ >= 0 && flock(fd_, LOCK_EX | LOCK_NB) == 0)
                locked_ = true;
        }
        ~FlockGuard()
        {
            if (fd_ >= 0) {
                if (locked_)
                    flock(fd_, LOCK_UN);
                close(fd_);
            }
        }
        FlockGuard(const FlockGuard &)            = delete;
        FlockGuard &operator=(const FlockGuard &) = delete;
        bool locked() const { return locked_; }

    private:
        int  fd_{ -1 };
        bool locked_{ false };
    };

protected:
    void SetUp() override
    {
        target_config = target_lookup("besm6");
        opt_flags     = opt_flags_default();
        input_file    = tmpfile();
        ASSERT_NE(nullptr, input_file);
    }

    void TearDown() override
    {
        fclose(input_file);
        if (program)
            free_program(program);
        symtab_destroy();
        structtab_destroy();
        typetab_destroy();
        nametab_destroy();
        xreport_lost_memory();
        EXPECT_EQ(xtotal_allocated_size(), 0);
        xfree_all();
    }

    // Disable optimization.
    void DisableOptimization() { opt_flags = {}; }

    // Capture Madlen output from a pre-built Besm_Module (used by Madlen-level tests).
    static std::string capture(const Besm_Module *module)
    {
        FILE *f = tmpfile();
        EXPECT_NE(nullptr, f);
        emit_madlen_module(f, module);
        long len = ftell(f);
        rewind(f);
        std::string result(static_cast<size_t>(len), '\0');
        EXPECT_TRUE(fread(&result[0], 1, static_cast<size_t>(len), f));
        fclose(f);
        return result;
    }

    // Capture emitted assembly from a pre-built TAC toplevel with full program context,
    // for the requested dialect.
    static std::string capture(const Tac_TopLevel *prog, const Tac_TopLevel *tl,
                               Besm_Dialect dialect = BESM_MADLEN)
    {
        FILE *f = tmpfile();
        EXPECT_NE(nullptr, f);
        codegen_program(prog, tl, f, dialect);
        long len = ftell(f);
        if (len == 0) {
            fclose(f);
            return {};
        }
        rewind(f);
        std::string result(static_cast<size_t>(len), '\0');
        EXPECT_TRUE(fread(&result[0], 1, static_cast<size_t>(len), f));
        fclose(f);
        return result;
    }

    // Backward-compatible overload: single pre-built toplevel acts as its own program.
    static std::string capture(const Tac_TopLevel *tl) { return capture(tl, tl); }

    // Parse C source, run full typecheck+translate+codegen pipeline, and return the
    // concatenated assembly (for the requested dialect) of every translated toplevel.
    std::string CompileTo(const char *src, Besm_Dialect dialect)
    {
        // Expand any #include/#define directives via the system cpp first so tests
        // can pull in the shipped standard headers; directive-free source is
        // returned unchanged.
        std::string source = preprocess_source(src);
        if (source.empty()) {
            ADD_FAILURE() << "C preprocessing failed for test source";
            return {};
        }
        fwrite(source.data(), 1, source.size(), input_file);
        rewind(input_file);
        program = parse(input_file);
        EXPECT_NE(nullptr, program);

        // Phase 1: translate all declarations and collect the full TAC chain.
        // The full chain is needed so frame_build can identify module-level names.
        Tac_TopLevel *all_tac = nullptr, **tac_tail = &all_tac;
        ExternalDecl *decls = program->decls;
        program->decls      = nullptr;
        int label_seq = 0; // unit-wide temp/label counter (see translate.h)
        while (decls) {
            ExternalDecl *next = decls->next;
            decls->next        = nullptr;
            typecheck_decl(decls, &label_seq);
            Tac_TopLevel *tac = translate(decls, opt_flags, &label_seq);
            free_external_decl(decls);
            if (tac) {
                Tac_TopLevel *t = tac;
                while (t->next)
                    t = t->next;
                *tac_tail = tac;
                tac_tail  = &t->next;
            }
            decls = next;
        }

        // Phase 2: codegen each toplevel with the full program chain as context.
        std::string result;
        for (const Tac_TopLevel *t = all_tac; t; t = t->next)
            result += capture(all_tac, t, dialect);
        tac_free_toplevel(all_tac);
        return result;
    }

    std::string CompileToMadlen(const char *src) { return CompileTo(src, BESM_MADLEN); }
    std::string CompileToUnix(const char *src) { return CompileTo(src, BESM_UNIX); }

    // Compile C source, run it under the Dubna simulator, and return the program output.
    // Returns "ERROR" on compile failure, simulator failure, or malformed listing.
    std::string CompileAndRun(const std::string &src)
    {
        std::string madlen = CompileToMadlen(src.c_str());

        std::string job =
            "*name .\n"
            "*disc:1/local\n"
            "*file:libc,40\n"
            "*call setftn:one,long\n"
            "*assem\n";
        job += madlen;
        job +=
            "*library:40\n"
            "*execute\n"
            "*end file\n";

        const char *test_name = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        std::string dub_path  = std::string(TEST_DIR "/") + test_name + ".dub";
        std::string lst_path  = std::string(TEST_DIR "/") + test_name + ".lst";

        // Held across the .dub write, the dubna run, and the .lst read; released by RAII
        // on every return below.  A failure to acquire means another besm-tests process
        // is running this same test concurrently and would clobber these files.
        FlockGuard dub_lock(dub_path);
        if (!dub_lock.locked()) {
            ADD_FAILURE() << "Concurrent besm-tests run detected for this test; do not "
                             "launch two besm-tests processes at once ("
                          << dub_path << ")";
            return "ERROR";
        }

        {
            std::ofstream dub(dub_path);
            if (!dub)
                return "ERROR";
            dub << job;
        }

        try {
            RunExternalProgram("dubna", { dub_path }, lst_path);
        } catch (...) {
            return "ERROR";
        }

        std::ifstream lst(lst_path);
        if (!lst)
            return "ERROR";
        std::string listing((std::istreambuf_iterator<char>(lst)), {});

        // Find second "≠" line (U+2260, UTF-8: 3 bytes).
        const std::string NE = "\xe2\x89\xa0";
        int ne_count         = 0;
        size_t pos           = 0;
        size_t content_start = std::string::npos;
        while (pos <= listing.size()) {
            size_t nl       = listing.find('\n', pos);
            size_t line_end = (nl == std::string::npos) ? listing.size() : nl;
            if (listing.substr(pos, line_end - pos) == NE) {
                if (++ne_count == 2) {
                    content_start = (nl == std::string::npos) ? listing.size() : nl + 1;
                    break;
                }
            }
            if (nl == std::string::npos)
                break;
            pos = nl + 1;
        }
        if (content_start == std::string::npos)
            return "ERROR";

        std::string content = listing.substr(content_start);

        // Truncate at the "----" separator line.
        pos = 0;
        while (pos < content.size()) {
            size_t nl       = content.find('\n', pos);
            size_t line_end = (nl == std::string::npos) ? content.size() : nl;
            if (content.substr(pos, line_end - pos).substr(0, 4) == "----") {
                content.resize(pos);
                break;
            }
            if (nl == std::string::npos)
                break;
            pos = nl + 1;
        }
        return content;
    }

    // Compile C source through the Unix (b6as) path, assemble it with b6as, and link it
    // with b6ld against the U2 libc.a.  Asserts each external step exits 0 (non-fatal
    // EXPECT, with the tool's captured diagnostics on failure).  Returns the emitted .s
    // text so a caller may additionally golden-diff it.  Execution under b6sim is out of
    // scope (tasks U5/U6) — this only proves the assembly assembles and links cleanly.
    std::string CompileAndAssembleUnix(const std::string &src)
    {
        std::string asm_text = CompileToUnix(src.c_str());

        const char *test_name = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        std::string base      = std::string(TEST_DIR "/") + test_name;
        std::string s_path    = base + ".s";
        std::string o_path    = base + ".o";
        std::string exe_path  = base + ".b6";
        std::string as_log    = base + ".aslog";
        std::string ld_log    = base + ".ldlog";

        // Held across the .s write, the assemble, and the link; released by RAII on every
        // return below.  A failure to acquire means another besm-tests process is running
        // this same test concurrently and would clobber these shared scratch files.
        FlockGuard lock(s_path);
        if (!lock.locked()) {
            ADD_FAILURE() << "Concurrent besm-tests run detected for this test; do not "
                             "launch two besm-tests processes at once ("
                          << s_path << ")";
            return asm_text;
        }

        {
            std::ofstream s(s_path);
            if (!s) {
                ADD_FAILURE() << "Cannot write " << s_path;
                return asm_text;
            }
            s << asm_text;
        }

        // genbesm --unix already ran in-process via CompileToUnix; now assemble, then link.
        int as_rc = RunTool({ "b6as", "-o", o_path, s_path }, as_log);
        EXPECT_EQ(0, as_rc) << "b6as failed on " << s_path << ":\n" << ReadFile(as_log);
        if (as_rc != 0)
            return asm_text;

        // Objects first, libc.a last so back-references resolve via the b6ranlib index.
        // libc.a is staged in the test's working directory (build/backend/besm6), which
        // besm-tests chdir()s into at startup, so a plain relative path suffices.
        int ld_rc = RunTool({ "b6ld", "-o", exe_path, o_path, "libc.a" }, ld_log);
        EXPECT_EQ(0, ld_rc) << "b6ld failed linking " << o_path << ":\n" << ReadFile(ld_log);

        return asm_text;
    }

    // Compile C source through the Unix (b6as) path, assemble with b6as, link with b6ld
    // against libc.a, then run the executable under the b6sim simulator and return its
    // captured stdout.  The Unix-path counterpart of CompileAndRun (which uses the Madlen
    // .mad → dubna .lst path): b6sim writes the program's write(1,…) output straight to
    // stdout, so there is no listing to scrape.  Returns "ERROR" on any tool failure.
    std::string CompileAndRunUnix(const std::string &src)
    {
        std::string asm_text = CompileToUnix(src.c_str());

        const char *test_name = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        std::string base      = std::string(TEST_DIR "/") + test_name;
        std::string s_path    = base + ".s";
        std::string o_path    = base + ".o";
        std::string exe_path  = base + ".b6";
        std::string out_path  = base + ".out";
        std::string as_log    = base + ".aslog";
        std::string ld_log    = base + ".ldlog";

        // Held across the .s write, assemble, link, and run; released by RAII on every
        // return below.  A failure to acquire means another besm-tests process is running
        // this same test concurrently and would clobber these shared scratch files.
        FlockGuard lock(s_path);
        if (!lock.locked()) {
            ADD_FAILURE() << "Concurrent besm-tests run detected for this test; do not "
                             "launch two besm-tests processes at once ("
                          << s_path << ")";
            return "ERROR";
        }

        {
            std::ofstream s(s_path);
            if (!s) {
                ADD_FAILURE() << "Cannot write " << s_path;
                return "ERROR";
            }
            s << asm_text;
        }

        int as_rc = RunTool({ "b6as", "-o", o_path, s_path }, as_log);
        EXPECT_EQ(0, as_rc) << "b6as failed on " << s_path << ":\n" << ReadFile(as_log);
        if (as_rc != 0)
            return "ERROR";

        // crt0.o first: b6ld takes the entry point from the first object's first text word,
        // so the C startup object must lead, ahead of the program object and libc.a.  All
        // three are staged in the working directory (build/backend/besm6), which besm-tests
        // chdir()s into at startup, so plain relative names suffice.
        int ld_rc = RunTool({ "b6ld", "-o", exe_path, "crt0.o", o_path, "libc.a" }, ld_log);
        EXPECT_EQ(0, ld_rc) << "b6ld failed linking " << o_path << ":\n" << ReadFile(ld_log);
        if (ld_rc != 0)
            return "ERROR";

        // Run under the simulator, capturing the program's stdout to out_path.
        try {
            RunExternalProgram("b6sim", { exe_path }, out_path);
        } catch (...) {
            return "ERROR";
        }
        return ReadFile(out_path);
    }

    // Fork a child, exec prog_path with input_filenames as arguments,
    // and redirect its stdout to output_filename.
    // Throws std::runtime_error on any failure.
    static void RunExternalProgram(const std::string &prog_path,
                                   const std::vector<std::string> &input_filenames,
                                   const std::string &output_filename)
    {
        enum {
            STATUS_OK              = EXIT_SUCCESS,
            STATUS_COMPILER_FAILED = EXIT_FAILURE,
            STATUS_CANNOT_READ_INPUT,
            STATUS_CANNOT_WRITE_OUTPUT,
            STATUS_CANNOT_RUN_PROGRAM,
        };

        pid_t pid = fork();
        if (pid < 0)
            throw std::runtime_error("Cannot fork");

        if (pid == 0) {
            int in_fd = open(input_filenames[0].c_str(), O_RDONLY);
            if (in_fd < 0)
                exit(STATUS_CANNOT_READ_INPUT);
            close(in_fd);

            int out_fd = open(output_filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (out_fd < 0)
                exit(STATUS_CANNOT_WRITE_OUTPUT);
            dup2(out_fd, STDOUT_FILENO);
            close(out_fd);

            auto argv = build_argv(prog_path, input_filenames);
            execvp(argv[0], const_cast<char *const *>(argv.data()));
            exit(STATUS_CANNOT_RUN_PROGRAM);
        }

        int wait_status;
        if (waitpid(pid, &wait_status, 0) < 0)
            throw std::runtime_error("Lost child process #" + std::to_string(pid));

        int exit_code = WEXITSTATUS(wait_status);
        switch (exit_code) {
        case STATUS_OK:
            return;
        case STATUS_CANNOT_READ_INPUT:
            throw std::runtime_error("Cannot read " + input_filenames[0]);
        case STATUS_CANNOT_WRITE_OUTPUT:
            throw std::runtime_error("Cannot write " + output_filename);
        case STATUS_CANNOT_RUN_PROGRAM:
            throw std::runtime_error("Cannot execute " + prog_path);
        default:
            throw std::runtime_error("Program failed with status " + std::to_string(exit_code));
        }
    }

    // Run a tool with an explicit argv (argv[0] resolved on PATH via execvp), capturing its
    // combined stdout+stderr into log_path.  Returns the child's exit code (0 on success),
    // or -1 if fork/waitpid failed.  Unlike RunExternalProgram this puts the real output on
    // the tool's own -o argument, so it fits b6as/b6ld's "-o outfile first" command form and
    // preserves their diagnostics for the failure message.
    static int RunTool(const std::vector<std::string> &argv, const std::string &log_path)
    {
        pid_t pid = fork();
        if (pid < 0)
            return -1;

        if (pid == 0) {
            int log_fd = open(log_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (log_fd < 0)
                _exit(127);
            dup2(log_fd, STDOUT_FILENO);
            dup2(log_fd, STDERR_FILENO);
            close(log_fd);

            std::vector<const char *> cargv;
            cargv.reserve(argv.size() + 1);
            std::transform(argv.begin(), argv.end(), std::back_inserter(cargv),
                           [](const std::string &s) { return s.c_str(); });
            cargv.push_back(nullptr);
            execvp(cargv[0], const_cast<char *const *>(cargv.data()));
            _exit(127);
        }

        int status;
        if (waitpid(pid, &status, 0) < 0)
            return -1;
        return WEXITSTATUS(status);
    }

    // Read an entire file into a string (empty string if it cannot be opened).
    static std::string ReadFile(const std::string &path)
    {
        std::ifstream f(path);
        if (!f)
            return {};
        return std::string((std::istreambuf_iterator<char>(f)), {});
    }

    // True if an executable named `name` is found on PATH.  Used to skip the Unix
    // assemble+link tests when the sibling v7besm toolchain is not installed.
    static bool tool_available(const std::string &name)
    {
        const char *path = getenv("PATH");
        if (!path)
            return false;
        std::string p(path);
        size_t start = 0;
        while (start <= p.size()) {
            size_t colon    = p.find(':', start);
            size_t len      = (colon == std::string::npos) ? std::string::npos : colon - start;
            std::string dir = p.substr(start, len);
            if (!dir.empty() && access((dir + "/" + name).c_str(), X_OK) == 0)
                return true;
            if (colon == std::string::npos)
                break;
            start = colon + 1;
        }
        return false;
    }

private:
    // Build a null-terminated argv vector: [prog_path, file0, file1, ..., nullptr].
    static std::vector<const char *> build_argv(const std::string &prog,
                                                const std::vector<std::string> &files)
    {
        std::vector<const char *> argv;
        argv.reserve(files.size() + 2);
        argv.push_back(prog.c_str());
        std::transform(files.begin(), files.end(), std::back_inserter(argv),
                       [](const std::string &s) { return s.c_str(); });
        argv.push_back(nullptr);
        return argv;
    }
};

// Skip a Unix assemble+link test when the sibling v7besm b6as/b6ld tools are not installed
// on PATH, so `make run` stays green on machines without that toolchain.  Must be used at
// test-body scope: GTEST_SKIP()'s early return exits the whole test, not just a helper.
#define SKIP_IF_NO_UNIX_TOOLS()                                                          \
    do {                                                                                 \
        if (!tool_available("b6as") || !tool_available("b6ld"))                          \
            GTEST_SKIP() << "b6as/b6ld not on PATH; skipping Unix assemble+link test";   \
    } while (0)

// Like SKIP_IF_NO_UNIX_TOOLS() but also requires the b6sim simulator, for the Unix run
// harness (CompileAndRunUnix) which additionally executes the linked b.out.
#define SKIP_IF_NO_UNIX_RUN_TOOLS()                                                      \
    do {                                                                                 \
        if (!tool_available("b6as") || !tool_available("b6ld") ||                        \
            !tool_available("b6sim"))                                                    \
            GTEST_SKIP() << "b6as/b6ld/b6sim not on PATH; skipping Unix run test";       \
    } while (0)

//
// Shared helper for the imported "Writing a C Compiler" run tests
// (chapterNN_tests.cpp).
//
// The book's positive programs define `int main(void)` and are validated by
// their return value (an exit code, or 0 on success for self-checking tests).
// The BESM-6 libc entry point calls `void program()`, so we wrap each program
// with a program() that prints main()'s return value; the expected value is the
// stdout of the same wrapped source compiled and run with the host compiler.
//
// Wrap a book program so program() prints `main()`'s return value as "%d\n".
inline std::string WrapMain(const std::string &program)
{
    return "int printf(const char *format, ...);\n" + program +
           "\nvoid program(void) { printf(\"%d\\n\", main()); }\n";
}
