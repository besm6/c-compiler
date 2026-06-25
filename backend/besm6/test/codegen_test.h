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

    // Capture Madlen output from a pre-built TAC toplevel with full program context.
    static std::string capture(const Tac_TopLevel *prog, const Tac_TopLevel *tl)
    {
        FILE *f = tmpfile();
        EXPECT_NE(nullptr, f);
        codegen_program(prog, tl, f);
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

    // Parse C source, run full typecheck+translate+codegen pipeline, and
    // return the concatenated Madlen assembly for every translated toplevel.
    std::string CompileToMadlen(const char *src)
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
        while (decls) {
            ExternalDecl *next = decls->next;
            decls->next        = nullptr;
            typecheck_decl(decls);
            Tac_TopLevel *tac = translate(decls, opt_flags);
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
            result += capture(all_tac, t);
        tac_free_toplevel(all_tac);
        return result;
    }

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
