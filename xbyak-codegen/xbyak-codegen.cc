#include <xbyak/xbyak.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

class CodeGen {
   private:
    struct Func {
        std::string name;
        std::string res;
        std::vector<std::string> args;
        std::vector<uint8_t> bytes;
    };

    std::vector<Func> functions;

   public:
    template <typename Code>
    void add(std::string name, std::string res, std::vector<std::string> args) {
        Code code;
        code.ready();
        const uint8_t *bytes = code.getCode();
        functions.push_back(
            {std::move(name), res, args,
             std::vector<uint8_t>(bytes, bytes + code.getSize())});
    }

    std::string gen_source() const {
        std::string code;
        code += "#include <sys/mman.h>\n";
        code += "#include <unistd.h>\n";
        code += "\n";
        for (auto &func : functions) {
            code +=
                "extern \"C\" __attribute__((aligned(4096))) const unsigned "
                "char " +
                func.name + "[] = {\n";
            for (size_t i = 0; i < func.bytes.size(); i++) {
                uint8_t byte = func.bytes[i];
                auto hex = [](uint8_t num) {
                    return char(num < 10 ? '0' + num : 'a' + num - 10);
                };
                code += std::string() + "0x" + hex(byte / 16) + hex(byte % 16) +
                        ",";
                if (i % 16 == 15 || i == func.bytes.size() - 1) {
                    code += "\n";
                }
            }
            code += "};\n";
        }
        code += "\n";
        code += "__attribute__((constructor)) static void codegen_init_() {\n";
        code += "    long page_size = sysconf(_SC_PAGESIZE) - 1;\n";
        for (auto &func : functions) {
            code += "    mprotect((void*)" + func.name + ", (sizeof(" +
                    func.name +
                    ") + page_size) & ~page_size, PROT_READ | PROT_EXEC);\n";
        }
        code += "}\n";
        return code;
    }

    std::string gen_header() const {
        std::string code;
        code += "#pragma once\n";
        code += "\n";
        for (auto &func : functions) {
            code += "extern \"C\" auto " + func.name + "(";
            auto &args = func.args;
            for (auto it = args.begin(); it != args.end(); ++it) {
                code += *it;
                if (std::next(it) != args.end()) {
                    code += ", ";
                }
            }
            code += ") -> " + func.res + ";\n";
        }
        return code;
    }
};

struct Example : Xbyak::CodeGenerator {
    Example() : Xbyak::CodeGenerator(1024, Xbyak::AutoGrow) {
        mov(eax, 114514);
        ret();
    }
};

struct Add : Xbyak::CodeGenerator {
    Add() : Xbyak::CodeGenerator(1024, Xbyak::AutoGrow) {
        mov(eax, 0);
        jmp(".L3");
        L(".L4");
        movsxd(rcx, eax);
        mov(r8d, dword[rsi + rcx * 4]);
        add(dword[rdi + rcx * 4], r8d);
        add(eax, 1);
        L(".L3");
        cmp(eax, edx);
        jl(".L4");
        ret();
    }
};

int main() {
    CodeGen codegen;
    codegen.add<Example>("example", "int", {});
    codegen.add<Add>("add", "void", {"int *", "int *", "int"});
    std::string source = codegen.gen_source();
    std::string header = codegen.gen_header();
    printf("%s", source.c_str());
    printf("***\n");
    printf("%s", header.c_str());
    return 0;
}
