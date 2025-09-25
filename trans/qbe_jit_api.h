//
// Created by liujilan on 2025/9/25.
//

#ifndef MY_MINI_RV32IMA_QBE_JIT_API_H
#define MY_MINI_RV32IMA_QBE_JIT_API_H

// qbe_jit_api.h
#pragma once
#include <string>
#include <optional>
#include <filesystem>
#include <vector>
#include <stdexcept>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/wait.h>

namespace qbejit {

namespace fs = std::filesystem;

// 导出的 JIT 函数签名：与你的 SSA 约定一致
// QBE: export function w $<name>(p %state, p %ram, w %pc_in)
// C++: int (*JitFn)(void* state, void* ram, uint32_t pc_in)
using JitFn = int(*)(void*, void*, uint32_t);

// 生成进程唯一的“全局父目录”（首次调用用 mkdtemp 创建一次）
inline const std::string& default_parent() {
    static std::string cached = []{
        char templ[] = "/tmp/qbe_test_XXXXXX";
        char* d = mkdtemp(templ);
        if (!d) throw std::runtime_error("mkdtemp failed");
        return std::string(d);
    }();
    return cached;
}

// 将 PC 转为规范函数名：pc_XXXXXXXX
inline std::string pc_to_name(uint32_t pc) {
    char buf[32];
    snprintf(buf, sizeof(buf), "pc_%08x", pc);
    return std::string(buf);
}

// 小工具：fork/exec 调命令（避免 shell 注入）
inline void run_cmd(const std::vector<std::string>& argv) {
    std::vector<char*> args; args.reserve(argv.size()+1);
    for (auto& s : argv) args.push_back(const_cast<char*>(s.c_str()));
    args.push_back(nullptr);
    pid_t pid = fork();
    if (pid < 0) throw std::runtime_error("fork failed");
    if (pid == 0) { execvp(args[0], args.data()); _exit(127); }
    int st = 0; if (waitpid(pid, &st, 0) < 0) throw std::runtime_error("waitpid failed");
    if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) throw std::runtime_error("command failed: " + argv[0]);
}

// 构建产物路径
struct Paths {
    std::string dir, ssa, s, so;
};
inline Paths make_paths(const std::string& parent, const std::string& name) {
    fs::path d = fs::path(parent) / name;
    return { d.string(),
             (d / (name + ".ssa")).string(),
             (d / (name + ".s")).string(),
             (d / (name + ".so")).string() };
}

// 1) 生成新的 .so（写入 ssa → qbe → cc）; 如目录不存在则创建
inline void build_so(const std::string& parent, const std::string& name, const std::string& ssa_source) {
    auto P = make_paths(parent, name);
    std::error_code ec; fs::create_directories(P.dir, ec);
    if (ec) throw std::runtime_error("mkdir failed: " + ec.message());
    // 写 .ssa
    { FILE* f = fopen(P.ssa.c_str(), "wb");
      if (!f) throw std::runtime_error("open ssa failed");
      fwrite(ssa_source.data(), 1, ssa_source.size(), f);
      fclose(f); }
    // qbe -> .s
    run_cmd({ "qbe", "-o", P.s.c_str(), P.ssa.c_str() });
    // cc -> .so
    run_cmd({ "cc", "-fPIC", "-shared", P.s.c_str(), "-o", P.so.c_str() });
}

// 2) 加载并返回函数指针（不持久保存句柄，交由调用方管理）
struct Handle { void* h=nullptr; std::string so_abs; std::string name; };
inline std::pair<Handle,JitFn> load_fn(const std::string& parent, const std::string& name) {
    auto P = make_paths(parent, name);
    std::string so_abs = fs::absolute(P.so).string();
    void* h = dlopen(so_abs.c_str(), RTLD_NOW);
    if (!h) throw std::runtime_error(std::string("dlopen failed: ")+dlerror());
    void* sym = dlsym(h, name.c_str());  // 注意：QBE里是 $name，但输出符号名为 name
    if (!sym) { std::string e = dlerror(); dlclose(h); throw std::runtime_error("dlsym failed: "+e); }
    return { Handle{h, so_abs, name}, reinterpret_cast<JitFn>(sym) };
}

// 3) 卸载 .so（不删除磁盘）
inline void unload(Handle& hd) { if (hd.h) { dlclose(hd.h); hd.h=nullptr; } }

// 4) 彻底删除磁盘文件（慎用：会清理整个 name 子目录）
inline void purge_disk(const std::string& parent, const std::string& name) {
    auto P = make_paths(parent, name);
    std::error_code ec; fs::remove_all(P.dir, ec);
    if (ec) throw std::runtime_error("remove_all failed: " + ec.message());
}

} // namespace qbejit


#endif //MY_MINI_RV32IMA_QBE_JIT_API_H