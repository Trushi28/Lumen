#include "lumen/compiler.hpp"
#include "lumen/vm.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Declared in parser.cpp
namespace lumen { std::vector<lumen::StmtPtr> parse(const std::string& src); }

using namespace lumen;

static bool g_debug = false;

// ── Run one chunk of source ───────────────────────────────────────────────────
static InterpResult runSource(const std::string& source, VM& vm) {
    try {
        auto ast = lumen::parse(source);
        Compiler compiler;
        auto fn = compiler.compile(ast);
        if (g_debug) fn->chunk->disassemble("<script>");
        return vm.run(fn);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error: %s\n", e.what());
        return InterpResult::COMPILE_ERROR;
    }
}

// ── REPL ─────────────────────────────────────────────────────────────────────
static void repl() {
    VM vm;
    std::string line;
    std::printf("Lumen 0.1.0  —  type 'quit' to exit\n");
    while (true) {
        std::printf("» ");
        std::fflush(stdout);
        if (!std::getline(std::cin, line)) { std::printf("\n"); break; }
        if (line == "quit" || line == "exit") break;
        if (!line.empty()) runSource(line, vm);
    }
}

// ── File runner ───────────────────────────────────────────────────────────────
static int runFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) { std::fprintf(stderr, "Cannot open '%s'\n", path.c_str()); return 1; }
    std::ostringstream ss; ss << f.rdbuf();
    VM vm;
    auto res = runSource(ss.str(), vm);
    return res == InterpResult::OK ? 0 : 1;
}

// ── Entry point ───────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv + 1, argv + argc);

    // Parse flags
    std::string file;
    for (const auto& a : args) {
        if (a == "--debug" || a == "-d") g_debug = true;
        else if (a == "--help" || a == "-h") {
            std::printf("Usage: lumen [--debug] [file.lm]\n");
            return 0;
        }
        else file = a;
    }

    return file.empty() ? (repl(), 0) : runFile(file);
}
