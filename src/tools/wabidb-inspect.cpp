#include "instrumenter.hpp"
#include <cassert>
#include <fstream>
#include <ios>
#include <tools/tool-options.h>
#include <tools/tool-utils.h>
#include <unistd.h>
#include <wasm-type.h>
#include "common_wasm_func.hpp"
#include "operation-builder.hpp"
using namespace wasm_instrument;

enum InspectState {
    idle = 0,
    listing,
    positioning,
    commanding,
    instrumenting,
    executing,
    end,
};

using v128_t = struct{ 
    char data[16];
};

std::ostream& operator<<(std::ostream& o, v128_t &t) {
    for (auto i = 0; i < 16; i++) {
        o << std::hex << std::showbase << (int)t.data[i] << std::noshowbase << std::dec << ((i < 15) ? " " : "");
    }
    return o;
}

template <typename T>
T swap_endian(T u) {
    uint16_t temp = 0x0001;
    uint8_t* temp_s = (uint8_t*)&temp;
    if (temp_s[0]) return u;
    union {
        T u;
        unsigned char u8[sizeof(T)];
    } src, dst;
    src.u = u;
    for (size_t k = 0; k < sizeof(T); k++) {
        dst.u8[k] = src.u8[sizeof(T) - k - 1];
    }
    return dst.u;
}

class InspectPrintInfo {
public:
    enum Type {
        None = 0,
        local,
        global,
        backtrace,
    };
    class PrintInfo {
        public:
        size_t num;
        std::vector<wasm::Type> types;
        std::vector<std::string> names;
        virtual ~PrintInfo() {};
    };
    class LocalPrintInfo: public PrintInfo {
        public:
        size_t param_num;
    };
    class GlobalPrintInfo: public PrintInfo {
        public:
    };
    class BacktracePrintInfo: public PrintInfo {
        public:
        std::map<std::string, size_t> funcname_map;
    };
    PrintInfo* info;
    Type type;
    InspectPrintInfo() {
        type = Type::None;
    }
    InspectPrintInfo(Type _t) {
        type = _t;
        if (_t == Type::local) {
            info = new LocalPrintInfo;
        } else if (_t == Type::global) {
            info = new GlobalPrintInfo;
        } else if (_t == Type::backtrace) {
            info = new BacktracePrintInfo;
        }else assert(false);
    }
    void print() const {
        if (this->type == Type::local) {
            auto linfo = dynamic_cast<LocalPrintInfo*>(this->info);
            std::printf("locals:\n");
            for (size_t i = 0; i < linfo->num; i++) {
                if (linfo->types[i] == wasm::Type::none) continue;
                if (i < linfo->param_num) {
                    std::printf("%ld: param name: %s ", i, linfo->names[i].c_str());
                } else {
                    std::printf("%ld: var name: %s ", i, linfo->names[i].c_str());
                }
                std::cout << linfo->types[i] << std::endl;
            }
        } else if (this->type == Type::global) {
            auto ginfo = dynamic_cast<GlobalPrintInfo*>(this->info);
            std::printf("globals:\n");
            for (size_t i = 0; i < ginfo->num; i++) {
                if (ginfo->types[i] == wasm::Type::none) continue;
                std::printf("%ld: name: %s ", i, ginfo->names[i].c_str());
                std::cout << ginfo->types[i] << std::endl;
            }
        } else if (this->type == Type::backtrace) {
            auto binfo = dynamic_cast<BacktracePrintInfo*>(this->info);
            std::printf("backtrace:\n");
        } else {
            std::printf("None\n");
        }
    }
    void print(const std::string &filename) const {
        std::ifstream ifile(filename, std::ios::binary);
        if (this->type == Type::local) {
            auto linfo = dynamic_cast<LocalPrintInfo*>(this->info);
            std::printf("(wabidb-inspect) Locals:\n");
            for (size_t i = 0; i < linfo->num; i++) {
                if (linfo->types[i] == wasm::Type::none) continue;
                if (i < linfo->param_num) {
                    std::printf(" %ld: param name: $%s = ", i, linfo->names[i].c_str());
                } else {
                    std::printf(" %ld: var   name: $%s = ", i, linfo->names[i].c_str());
                }
                std::cout << linfo->types[i];
                _print_typed_number(ifile, linfo->types[i]);
            }
        } else if (this->type == Type::global) {
            auto ginfo = dynamic_cast<GlobalPrintInfo*>(this->info);
            std::printf("(wabidb-inspect) Globals:\n");
            for (size_t i = 0; i < ginfo->num; i++) {
                if (ginfo->types[i] == wasm::Type::none) continue;
                std::printf(" %ld: name: $%s = ", i, ginfo->names[i].c_str());
                std::cout << ginfo->types[i];
                _print_typed_number(ifile, ginfo->types[i]);
            }
        } else if (this->type == Type::backtrace) {
            auto binfo = dynamic_cast<BacktracePrintInfo*>(this->info);
            std::printf("(wabidb-inspect) Backtrace:\n");
            int32_t number_le;
            std::vector<int> backtrace_idx;
            while(!ifile.eof()) {
                ifile.read((char *)(&number_le), 4);
                if (ifile.eof()) break;
                int32_t number_be = swap_endian(number_le);
                assert(number_be != -2);
                if (number_be != -1) {
                    backtrace_idx.emplace_back(number_be);
                } else {
                    backtrace_idx.pop_back();
                }
            }
            for (int i = backtrace_idx.size() - 1; i >= 0; i--) {
                std::printf(" %ld: $%s\n", backtrace_idx.size() - 1 - i, binfo->names[backtrace_idx[i]].c_str());
            }
            std::printf(" %ld: $%s\n", backtrace_idx.size(), "_start (or what runtime directly call)");
        } else {
            std::printf("None\n");
        }
        ifile.close();
    }
private:
    void _print_typed_number(std::ifstream &ifile, wasm::Type type) const{
        if (type == wasm::Type::i32) {
            int32_t number_le;
            ifile.read((char *)(&number_le), 4);
            int32_t number_be = swap_endian(number_le);
            std::printf("(%d)\n", number_be);
        } else if (type == wasm::Type::i64) {
            int64_t number_le;
            ifile.read((char *)(&number_le), 8);
            int64_t number_be = swap_endian(number_le);
            std::printf("(%ld)\n", number_be);
        } else if (type == wasm::Type::f32) {
            assert(sizeof(float) == 4);
            float number_le;
            ifile.read((char *)(&number_le), 4);
            float number_be = swap_endian(number_le);
            std::printf("(%f)\n", number_be);
        } else if (type == wasm::Type::f64) {
            assert(sizeof(double) == 8);
            double number_le;
            ifile.read((char *)(&number_le), 8);
            double number_be = swap_endian(number_le);
            std::printf("(%lf)\n", number_be);
        } else if (type == wasm::Type::v128) {
            v128_t number_le;
            ifile.read((char *)(&number_le), 16);
            v128_t number_be = swap_endian(number_le);
            std::cout << "(" << number_be << ")" << std::endl;
        } else assert(false);
    }
};

static int get_max_func_line_num(const wasm::Module &m) {
    int ret = 0;
    for (const auto &f : m.functions) {
        if (f->stackIR == nullptr) continue;
        ret = std::max(ret, int(f->stackIR->size()));
    }
    return ret;
}

static std::string make_indent(int size, int line_num = -1) {
    if (line_num < 0) {
        return std::string(size, ' ');
    } else {
        std::string line_str = std::to_string(line_num);
        std::string space = std::string(std::max(0, (size - 1) - int(line_str.size())), ' ');
        return (space + line_str + ' ');
    }
}

static std::string make_indent_c(int size, int line_num = -1) {
    std::string color = "\033[1;37m";
    std::string normal = "\033[0m";
    if (line_num < 0) {
        return color + std::string(size, ' ') + normal;
    } else {
        std::string line_str = std::to_string(line_num);
        std::string space = std::string(std::max(0, (size - 1) - int(line_str.size())), ' ');
        return color + (space + line_str + ' ') + normal;
    }
}

static std::string first_none_space(const std::string &s, size_t len) {
    size_t pos = 0;
    while((pos < s.size()) && s[pos] == ' ') pos++;
    return s.substr(pos, std::min(len, (s.size() - pos)));
}

static void listing_module(const std::vector<std::string> &module_vec_str, int max_func_line_num, bool with_color) {
    // 0: normal(out func)
    // 1: in func
    // 2: after locals
    int state = 0;
    int line_num = 0;
    int indent_size = std::to_string(max_func_line_num).size() + 1;
    std::string func_prefix = with_color ? "(\033[31m\033[1mfunc" : "(func";
    std::string local_prefix = with_color ? "(\033[33mlocal" : "(local";
    std::function _make_indent = with_color ? make_indent_c : make_indent;
    for (const auto &line : module_vec_str) {
        std::string l = _make_indent(indent_size, -1) + line;
        switch (state) {
            case 0: {  //out func
                if (first_none_space(line, func_prefix.size()) == func_prefix) state = 1;
                else state = 0;
                break;
            }
            case 1: { //in func
                if (first_none_space(line, local_prefix.size()) != local_prefix) {
                    state = 2;
                    line_num = 1;
                } 
                else {
                    state = 1;
                    break;
                }
            }
            case 2: { //after locals
                if (first_none_space(line, 1) == ")") {
                    state = 0;
                } else {
                    l = _make_indent(indent_size, line_num) + line;
                    line_num++;
                }
                break;
            }
            default: assert(false);
        }
        std::printf("%s", l.c_str());
    }
}

static bool validate_inspect_pos(wasm::Module &m, const std::string func_name, size_t line_num) {
    auto func = m.getFunctionOrNull(func_name);
    if (func == nullptr) return false;
    if (func->stackIR == nullptr) return false;
    if (func->stackIR->size() < line_num) return false;
    return true;
}

static void _add_imports(Instrumenter &instrumenter, CommonWasmBuilder &wasm_builder) {
    // import WASI function fd_prestat_get()
    auto func = instrumenter.getImport(wasm::ModuleItemKind::Function, "fd_prestat_get");
    if (func == nullptr) {
        BinaryenType ii[2] = {BinaryenTypeInt32(), BinaryenTypeInt32()};
        BinaryenType fd_prestat_get_params = BinaryenTypeCreate(ii, 2);
        instrumenter.addImportFunction("__imported_wasi_snapshot_preview1_fd_prestat_get", "wasi_snapshot_preview1", "fd_prestat_get", 
                                        fd_prestat_get_params, BinaryenTypeInt32());
        wasm_builder.updateWasiName("fd_prestat_get", "__imported_wasi_snapshot_preview1_fd_prestat_get");
    } else {
        if (func->hasExplicitName) {
            wasm_builder.updateWasiName("fd_prestat_get", func->name.toString());
        } else {
            func->setExplicitName("__imported_wasi_snapshot_preview1_fd_prestat_get");
            wasm_builder.updateWasiName("fd_prestat_get", "__imported_wasi_snapshot_preview1_fd_prestat_get");
        }
    }

    // import WASI function fd_prestat_dir_name()
    func = instrumenter.getImport(wasm::ModuleItemKind::Function, "fd_prestat_dir_name");
    if (func == nullptr) {
        BinaryenType iii[3] = {BinaryenTypeInt32(), BinaryenTypeInt32(), BinaryenTypeInt32()};
        BinaryenType fd_prestat_dir_name_params = BinaryenTypeCreate(iii, 3);
        instrumenter.addImportFunction("__imported_wasi_snapshot_preview1_fd_prestat_dir_name", "wasi_snapshot_preview1", "fd_prestat_dir_name", 
                                        fd_prestat_dir_name_params, BinaryenTypeInt32());
        wasm_builder.updateWasiName("fd_prestat_dir_name", "__imported_wasi_snapshot_preview1_fd_prestat_dir_name");
    } else {
        if (func->hasExplicitName) {
            wasm_builder.updateWasiName("fd_prestat_dir_name", func->name.toString());
        } else {
            func->setExplicitName("__imported_wasi_snapshot_preview1_fd_prestat_dir_name");
            wasm_builder.updateWasiName("fd_prestat_dir_name", "__imported_wasi_snapshot_preview1_fd_prestat_dir_name");
        }
    }

    // import WASI function path_open()
    func = instrumenter.getImport(wasm::ModuleItemKind::Function, "path_open");
    if (func == nullptr) {
        BinaryenType i9[9] = {BinaryenTypeInt32(), BinaryenTypeInt32(), BinaryenTypeInt32(), BinaryenTypeInt32(), 
                            BinaryenTypeInt32(), BinaryenTypeInt64(), BinaryenTypeInt64(), BinaryenTypeInt32(), 
                            BinaryenTypeInt32()};
        BinaryenType path_open_params = BinaryenTypeCreate(i9, 9);
        instrumenter.addImportFunction("__imported_wasi_snapshot_preview1_path_open", "wasi_snapshot_preview1", "path_open", 
                                        path_open_params, BinaryenTypeInt32());
        wasm_builder.updateWasiName("path_open", "__imported_wasi_snapshot_preview1_path_open");
    } else {
        if (func->hasExplicitName) {
            wasm_builder.updateWasiName("path_open", func->name.toString());
        } else {
            func->setExplicitName("__imported_wasi_snapshot_preview1_path_open");
            wasm_builder.updateWasiName("path_open", "__imported_wasi_snapshot_preview1_path_open");
        }
    }

    // import WASI function fd_write()
    func = instrumenter.getImport(wasm::ModuleItemKind::Function, "fd_write");
    if (func == nullptr) {
        BinaryenType iiii[4] = {BinaryenTypeInt32(), BinaryenTypeInt32(), BinaryenTypeInt32(), BinaryenTypeInt32()};
        BinaryenType fd_write_params = BinaryenTypeCreate(iiii, 4);
        instrumenter.addImportFunction("__imported_wasi_snapshot_preview1_fd_write", "wasi_snapshot_preview1", "fd_write", 
                                        fd_write_params, BinaryenTypeInt32());
        wasm_builder.updateWasiName("fd_write", "__imported_wasi_snapshot_preview1_fd_write");
    } else {
        if (func->hasExplicitName) {
            wasm_builder.updateWasiName("fd_write", func->name.toString());
        } else {
            func->setExplicitName("__imported_wasi_snapshot_preview1_fd_write");
            wasm_builder.updateWasiName("fd_write", "__imported_wasi_snapshot_preview1_fd_write");
        }
    }

    // import WASI function fd_close()
    func = instrumenter.getImport(wasm::ModuleItemKind::Function, "fd_close");
    if (func == nullptr) {
        instrumenter.addImportFunction("__imported_wasi_snapshot_preview1_fd_close", "wasi_snapshot_preview1", "fd_close", 
                                        BinaryenTypeInt32(), BinaryenTypeInt32());
        wasm_builder.updateWasiName("fd_close", "__imported_wasi_snapshot_preview1_fd_close");
    } else {
        if (func->hasExplicitName) {
            wasm_builder.updateWasiName("fd_close", func->name.toString());
        } else {
            func->setExplicitName("__imported_wasi_snapshot_preview1_fd_close");
            wasm_builder.updateWasiName("fd_close", "__imported_wasi_snapshot_preview1_fd_close");
        }
    }

    // import WASI function proc_exit()
    func = instrumenter.getImport(wasm::ModuleItemKind::Function, "proc_exit");
    if (func == nullptr) {
        instrumenter.addImportFunction("__imported_wasi_snapshot_preview1_proc_exit", "wasi_snapshot_preview1", "proc_exit", 
                                        BinaryenTypeInt32(), BinaryenTypeNone());
        wasm_builder.updateWasiName("proc_exit", "__imported_wasi_snapshot_preview1_proc_exit");
    } else {
        if (func->hasExplicitName) {
            wasm_builder.updateWasiName("proc_exit", func->name.toString());
        } else {
            func->setExplicitName("__imported_wasi_snapshot_preview1_proc_exit");
            wasm_builder.updateWasiName("proc_exit", "__imported_wasi_snapshot_preview1_proc_exit");
        }
    }
}

static void _add_globals(Instrumenter &instrumenter) {
    // memory-associate globals:
    // auto global_ret = instrumenter.addGlobal("__instr_page_addr", BinaryenTypeInt32(), true, BinaryenLiteralInt32(-1));
    // assert(global_ret != nullptr);
    auto global_ret = instrumenter.addGlobal("__instr_page_guide", BinaryenTypeInt32(), false, BinaryenLiteralInt32(1024));
    assert(global_ret != nullptr);
    global_ret = instrumenter.addGlobal("__instr_base_addr", BinaryenTypeInt32(), true, BinaryenLiteralInt32(-1));
    assert(global_ret != nullptr);
    global_ret = instrumenter.addGlobal("__instr_wasi_ret_addr", BinaryenTypeInt32(), true, BinaryenLiteralInt32(-1));
    assert(global_ret != nullptr);
    global_ret = instrumenter.addGlobal("__instr_iobuf_addr", BinaryenTypeInt32(), true, BinaryenLiteralInt32(-1));
    assert(global_ret != nullptr);

    // other globals:
    global_ret = instrumenter.addGlobal("__instr_iobuf_len", BinaryenTypeInt32(), true, BinaryenLiteralInt32(0));
    assert(global_ret != nullptr);
    global_ret = instrumenter.addGlobal("__instr_fd", BinaryenTypeInt32(), true, BinaryenLiteralInt32(-1));
    assert(global_ret != nullptr);
}

static void _add_memory(Instrumenter &instrumenter, std::string &memory_name) {
    auto memory_ret = instrumenter.getMemory();
    if (memory_ret == nullptr) {
        memory_ret = instrumenter.addMemory("mem", false, 0, 1);
        assert(memory_ret != nullptr);
    } else {
        memory_name = memory_ret->name.toString();
        if ((memory_ret->max - memory_ret->initial) <= 0) {
            memory_ret->max = std::min(static_cast<uint64_t>(memory_ret->max + 1), static_cast<uint64_t>(wasm::Memory::kMaxSize32));
            assert((memory_ret->max - memory_ret->initial) > 0);
        }
    }
}

static void _add_data_segments(Instrumenter &instrumenter) {
    auto data_ret = instrumenter.addPassiveDateSegment(".instr_rodata", ".\00", 2);
    assert(data_ret != nullptr);
    data_ret = instrumenter.addPassiveDateSegment(".instr_filename", "__instr_cache.file\00", 19);
    assert(data_ret != nullptr);
}

static void _add_functions(Instrumenter &instrumenter, CommonWasmBuilder &wasm_builder) {
    bool add_func_ret = instrumenter.addFunctions(
        {
            "__instr_memcmp",
            "__instr_get_cwd_fd",
            "__instr_fopen_rw",
            "__instr_load_data",
        }, 
        {
            wasm_builder.getWasmFunction("__instr_memcmp").value(),
            wasm_builder.getWasmFunction("__instr_get_cwd_fd").value(),
            wasm_builder.getWasmFunction("__instr_fopen_rw").value(),
            "(func $__instr_load_data\n"
            "i32.const 1\n"
            "memory.grow\n"
            "i32.const 65536\n"
            "i32.mul\n"
            "global.get $__instr_page_guide\n"
            "i32.add\n"
            "global.set $__instr_base_addr\n"
            
            "global.get $__instr_base_addr\n"
            "i32.const 3072\n"
            "i32.add\n"
            "global.set $__instr_wasi_ret_addr\n"
            
            "global.get $__instr_base_addr\n"
            "i32.const 4096\n"
            "i32.add\n"
            "global.set $__instr_iobuf_addr\n"
            
            "global.get $__instr_base_addr\n"
            "i32.const 0\n"
            "i32.const 2\n"
            "memory.init $.instr_rodata\n"
            
            "global.get $__instr_base_addr\n"
            "i32.const 1024\n"
            "i32.add\n"
            "i32.const 0\n"
            "i32.const 19\n"
            "memory.init $.instr_filename\n"
            ")",
        }
    );
    assert(add_func_ret == true);
}

static void _add_exports(Instrumenter &instrumenter, std::string &memory_name) {
    auto export_ret = instrumenter.getExport("memory");
    if (export_ret == nullptr) {
        export_ret = instrumenter.addExport(wasm::ModuleItemKind::Memory, memory_name.c_str(), "memory");
        assert(export_ret != nullptr);
    }
}

static void _make_variable_op(const InspectPrintInfo::PrintInfo &info, InstrumentOperation &op, const char cmd) {
    std::string item;
    if (cmd == 'l') {
        item = "local";
        op.post_instructions.local_types = info.types;
    } else if (cmd == 'g') {
        item = "global";
    } else assert(false);
    for (auto i = 0; i < info.num; i++) {
        if (info.types[i] == wasm::Type::none) continue;
        op.post_instructions.instructions.insert(op.post_instructions.instructions.end(), {
            "global.get $__instr_iobuf_addr",
            "global.get $__instr_iobuf_len",
            "i32.add",
            item + ".get " + std::to_string(i),
        });
        if (info.types[i] == wasm::Type::i32) {
            op.post_instructions.instructions.emplace_back("i32.store");
            op.post_instructions.instructions.emplace_back("i32.const 4");
        } else if (info.types[i] == wasm::Type::i64) {
            op.post_instructions.instructions.emplace_back("i64.store");
            op.post_instructions.instructions.emplace_back("i32.const 8");
        } else if (info.types[i] == wasm::Type::f32) {
            op.post_instructions.instructions.emplace_back("f32.store");
            op.post_instructions.instructions.emplace_back("i32.const 4");
        } else if (info.types[i] == wasm::Type::f64) {
            op.post_instructions.instructions.emplace_back("f64.store");
            op.post_instructions.instructions.emplace_back("i32.const 8");
        } else if (info.types[i] == wasm::Type::v128) {
            op.post_instructions.instructions.emplace_back("v128.store");
            op.post_instructions.instructions.emplace_back("i32.const 16");
        }
        op.post_instructions.instructions.insert(op.post_instructions.instructions.end(), {
            "global.get $__instr_iobuf_len",
            "i32.add",
            "global.set $__instr_iobuf_len",
        });
    }
}

static void _make_write_op(InstrumentOperation &op, const CommonWasmBuilder &builder) {
    op.post_instructions.instructions.insert(op.post_instructions.instructions.end(), 
    {
        // construct ciovec
        "global.get $__instr_base_addr",
        "i32.const 2048",
        "i32.add",
        "global.get $__instr_iobuf_addr",
        "i32.store",
        "global.get $__instr_base_addr",
        "i32.const 2052",
        "i32.add",
        "global.get $__instr_iobuf_len",
        "i32.store",
        // construct write part
        "global.get $__instr_base_addr",
        "global.get $__instr_wasi_ret_addr",
        "call $__instr_get_cwd_fd",
        "i32.const 0",
        "i32.ne",
        "if",
        "i32.const 12",
        "call $" + builder.getWasiName("proc_exit").value(),
        "end",

        "global.get $__instr_wasi_ret_addr",
        "i32.load",
        "global.get $__instr_base_addr",
        "i32.const 1024",
        "i32.add",
        "i32.const 18",
        "global.get $__instr_wasi_ret_addr",
        "call $__instr_fopen_rw",
        "i32.const 0",
        "i32.ne",
        "if",
        "i32.const 12",
        "call $" + builder.getWasiName("proc_exit").value(),
        "end",

        "global.get $__instr_wasi_ret_addr",
        "i32.load",
        "global.set $__instr_fd",
        "global.get $__instr_fd",
        "global.get $__instr_base_addr",
        "i32.const 2048",
        "i32.add",
        "i32.const 1",
        "global.get $__instr_wasi_ret_addr",
        "call $" + builder.getWasiName("fd_write").value(),
        "i32.const 0",
        "i32.ne",
        "if",
        "i32.const 12",
        "call $" + builder.getWasiName("proc_exit").value(),
        "end",

        "global.get $__instr_fd",
        "call $" + builder.getWasiName("fd_close").value(),
        "i32.const 0",
        "i32.ne",
        "if",
        "i32.const 12",
        "call $" + builder.getWasiName("proc_exit").value(),
        "end",
    });
}

static void _make_bt_instrument(Instrumenter &instrumenter, 
                                const InspectPrintInfo::BacktracePrintInfo &info, 
                                const std::string &inspect_func_name,
                                const size_t inspect_line_num) {
    InstrumentOperation hook_call;
    hook_call.pre_instructions.instructions = {
        "global.get $__instr_iobuf_addr",
        "global.get $__instr_iobuf_len",
        "i32.add",
        "i32.const -1", // to be modified
        "i32.store",
        "i32.const 4",
        "global.get $__instr_iobuf_len",
        "i32.add",
        "global.set $__instr_iobuf_len",
    };
    try {
        bool if_in_inspect_func = false;
        size_t line_num = 0;
        OperationBuilder builder;
        auto added_instructions = builder.makeOperations(instrumenter.getModule(), {hook_call});
        auto hook_insts = (*added_instructions)[0].pre_instructions;

        auto inst_vistor = [&hook_insts, &info, &instrumenter, &if_in_inspect_func, &line_num, &inspect_line_num]
                                    (std::list<wasm::StackInst*> &l, std::list<wasm::StackInst*>::iterator &iter) {
            if (if_in_inspect_func) {
                line_num++;
                if (line_num > inspect_line_num) return;
            }
            auto inst = *iter;
            if (inst->origin->_id == wasm::Expression::Id::CallId) {
                auto call = inst->origin->dynCast<wasm::Call>();
                auto idx_iter = info.funcname_map.find(call->target.toString());
                wasm::StackInst* const_neg_one = hook_insts[3];
                if (idx_iter != info.funcname_map.end()) {
                    hook_insts[3] = _make_stack_inst(wasm::StackInst::Basic, 
                                                    BinaryenConst(instrumenter.getModule(), 
                                                                BinaryenLiteralInt32((int32_t)(idx_iter->second))),
                                                    instrumenter.getModule());
                } else {
                    hook_insts[3] = _make_stack_inst(wasm::StackInst::Basic, 
                                                    BinaryenConst(instrumenter.getModule(), 
                                                                BinaryenLiteralInt32((int32_t)(-2))),
                                                    instrumenter.getModule());
                }
                l.splice(iter, _stack_ir_vec2list(hook_insts));
                hook_insts[3] = const_neg_one;
            } else {
                return;
            }
            std::advance(iter, 1);
            l.splice(iter, _stack_ir_vec2list(hook_insts));
            std::advance(iter, -1);
        };
        
        auto func_visitor = [&inst_vistor, &instrumenter, &inspect_func_name, &if_in_inspect_func](wasm::Function* func) {
            if (!instrumenter.scopeContains(func->name.toString())) return;
            if (func->name.toString() == inspect_func_name) if_in_inspect_func = true;
            iterInstructions(func, inst_vistor);
            if_in_inspect_func = false;
        };
        
        iterDefinedFunctions(instrumenter.getModule(), func_visitor);
    } catch(...) {
        assert(false);
    }

    auto start_func = instrumenter.getStartFunction();
    if (start_func != nullptr) {
        InstrumentOperation temp;
        temp.post_instructions.instructions.emplace_back("call $__instr_load_data");
        InstrumentResult iresult = instrumenter.instrumentFunction(temp, start_func->name.toString().c_str(), 0);
        assert(iresult == InstrumentResult::success);
    } else {
        instrumenter.getModule()->addStart("__instr_load_data");
    }
}

static void do_pre_instrument(Instrumenter &instrumenter,
                              const std::string &inspect_func_name,
                              const size_t inspect_line_num,
                              const std::string &inspect_command,
                              const InspectPrintInfo &print_info)
{
    // auto start_func = instrumenter.getStartFunction();
    // assert(start_func != nullptr);
    CommonWasmBuilder wasm_builder;
    _add_imports(instrumenter, wasm_builder);
    _add_globals(instrumenter);
    std::string memory_name = "mem";
    _add_memory(instrumenter, memory_name);
    _add_data_segments(instrumenter);
    _add_functions(instrumenter, wasm_builder);
    _add_exports(instrumenter, memory_name);

    InstrumentOperation op;
    if (inspect_command == "l" || inspect_command == "g") {
        op.post_instructions.instructions.emplace_back("call $__instr_load_data");
        _make_variable_op(*(print_info.info), op, inspect_command[0]);
    }
    _make_write_op(op, wasm_builder);
    op.post_instructions.instructions.emplace_back("i32.const 10");
    op.post_instructions.instructions.emplace_back("call $" + wasm_builder.getWasiName("proc_exit").value());
    InstrumentResult iresult = instrumenter.instrumentFunction(op, inspect_func_name.c_str(), inspect_line_num);
    assert(iresult == InstrumentResult::success);
    
    if (inspect_command == "bt") {
        _make_bt_instrument(instrumenter,
                            *dynamic_cast<InspectPrintInfo::BacktracePrintInfo*>(print_info.info),
                            inspect_func_name, inspect_line_num);
    }
    assert(BinaryenModuleValidate(instrumenter.getModule()));
}

static void modify_runtime_command(std::string &cmd, const std::string &wasm_file) {
    if ((cmd.find("--dir=.") == std::string::npos) && (cmd.find(R"("--dir=.")") == std::string::npos)) {
        auto pos = cmd.find(" ");
        if (pos == std::string::npos) {
            cmd += " --dir=.";
        } else {
            cmd.insert(pos, " --dir=.");
        }
    }
    auto pos = cmd.find(".wasm");
    if (pos == std::string::npos) {
        cmd += " ";
        cmd += wasm_file;
    } else {
        auto start_pos = cmd.rfind(" ", pos);
        assert(start_pos != std::string::npos);
        while (cmd[start_pos] == ' ') start_pos++;
        cmd.replace(start_pos, pos + 5 - start_pos, wasm_file);
    }
}

static bool run_runtime_command(const std::string &cmd) {
    int return_code = system(cmd.c_str());
    return_code = ((return_code) & 0xff00) >> 8;
    if (return_code == 10) {
        return true;
    } else if (return_code == 12) {
        std::printf("(wabidb-inspect) Instrumented part failed!\n");
    } else {
        std::printf("(wabidb-inspect) Unexpected return code: %d!\n", return_code);
    }
    return false;
}

static void print_cache_file(const InspectPrintInfo &info, const std::string &filename) {
    if (access(filename.c_str(), R_OK) == 0) {
        info.print(filename);
    } else {
        std::printf("(wabidb-inspect) Cache file cannot access!\n");
    }
}

int main(int argc, const char* argv[]) {
    const std::string WabidbInspectOption = "wabidb-inspect options";
    wasm::ToolOptions options("wabidb-inspect", "Make one point inspection into a wasm binary.");
    std::string command = "";

    options
    .add("--output",
         "-o",
         "Output instrumented wasm filename",
         WabidbInspectOption,
         wasm::Options::Arguments::One,
         [](wasm::Options* o, const std::string& argument) {
            o->extra["outfile"] = argument;
         })
    .add("--command",
         "-cmd",
         "The command to run the wasm file on a certain runtime",
         WabidbInspectOption,
         wasm::Options::Arguments::One,
         [&](wasm::Options* o, const std::string& argument) { command = argument; })
    .add_positional("INFILE",
                    wasm::Options::Arguments::One,
                    [](wasm::Options* o, const std::string& argument) {
                        o->extra["infile"] = argument;
                    });
    options.parse(argc, argv);
    if (options.extra.find("infile") == options.extra.end()) {
        std::cerr << "Usage: wabidb-inspect <INFILE>" << std::endl;
        return 1;
    }
    auto infile = options.extra["infile"];
    if ((infile.size() < 6) || (infile.substr(infile.size() - 5 , 5) != ".wasm")) {
        std::cerr << "INFILE must be a .wasm file" << std::endl;
        return 1;
    }
    if (options.extra.find("outfile") == options.extra.end()) {
        options.extra["outfile"] = wasm::removeSpecificSuffix(infile, ".wasm") + "-inspect.wasm";
    }

    // make config through CLI options
    InstrumentConfig config;
    config.filename = infile;
    config.targetname = options.extra["outfile"];
    wasm::Module* temp_module = new wasm::Module;
    temp_module->features = config.feature;
    options.applyFeatures(*temp_module);
    config.feature = temp_module->features;
    delete temp_module;

    Instrumenter instrumenter;
    InstrumentResult iresult = instrumenter.setConfig(config);
    assert(iresult == InstrumentResult::success);

    std::stringstream mstream;
    auto is_color = Colors::isEnabled();
    bool with_color = true;
    Colors::setEnabled(with_color);
    // do stack ir pass on the module
    wasm::printStackIR(mstream, instrumenter.getModule(), true);
    auto module_str = mstream.str();
    Colors::setEnabled(is_color);

    // split module string
    size_t pos = 0;
    std::vector<std::string> module_vec_str;
    while (pos < module_str.size()) {
        auto next_pos = module_str.find('\n', pos) + 1;
        module_vec_str.emplace_back(module_str.substr(pos, next_pos - pos));
        pos = next_pos;
    }

    InspectState state = InspectState::idle;
    bool if_end = false;
    int max_func_line_num = 0;
    std::string inspect_func_name;
    size_t inspect_line_num;
    std::string inspect_command;
    InspectPrintInfo* inspect_print_info = nullptr;
    while (!if_end) {
        switch (state) {
            case InspectState::idle:
            {
                max_func_line_num = get_max_func_line_num(*instrumenter.getModule());
                state = InspectState::listing;
                break;
            }
            case InspectState::listing:
            {
                std::printf("(wabidb-inspect) Listing\n");
                listing_module(module_vec_str, max_func_line_num, with_color);
                state = InspectState::positioning;
                break;
            }
            case InspectState::positioning:
            {
                std::printf("(wabidb-inspect) Enter inspect position\n");
                std::printf(" > func: ");
                std::cin >> inspect_func_name;
                std::printf(" > line: ");
                std::cin >> inspect_line_num;
                if (validate_inspect_pos(*instrumenter.getModule(), inspect_func_name, inspect_line_num)) {
                    state = InspectState::commanding;
                } else {
                    std::printf(" Error: please enter valid inspect position\n");
                    state = InspectState::positioning;
                }
                break;
            }
            case InspectState::commanding:
            {
                std::printf("(wabidb-inspect) Enter inspect command\n");
                std::printf(" > locals(l) | globals(g) | backtrace(bt)\n > ");
                std::cin >> inspect_command;
                if (inspect_command == "locals" || inspect_command == "l") {
                    inspect_command = "l";
                    // make print info based on command l
                    inspect_print_info = new InspectPrintInfo(InspectPrintInfo::Type::local);
                    auto target_func = instrumenter.getFunction(inspect_func_name.c_str());
                    auto linfo = dynamic_cast<InspectPrintInfo::LocalPrintInfo*>(inspect_print_info->info);
                    linfo->num = target_func->getNumLocals();
                    linfo->param_num = target_func->getNumParams();
                    for (size_t i = 0; i < linfo->num; i++) {
                        auto type = target_func->getLocalType(i);
                        linfo->types.emplace_back(type);
                        linfo->names.emplace_back(target_func->getLocalNameOrGeneric(i).toString());
                    }
                } else if (inspect_command == "globals" || inspect_command == "g") {
                    inspect_command = "g";
                    // make print info based on command g
                    inspect_print_info = new InspectPrintInfo(InspectPrintInfo::Type::global);
                    auto target_module = instrumenter.getModule();
                    auto ginfo = dynamic_cast<InspectPrintInfo::GlobalPrintInfo*>(inspect_print_info->info);
                    ginfo->num = target_module->globals.size();
                    for (size_t i = 0; i < ginfo->num; i++) {
                        auto type = target_module->globals[i]->type;
                        if (!type.isNumber()) type = wasm::Type::none;
                        ginfo->types.emplace_back(type);
                        ginfo->names.emplace_back(target_module->globals[i]->name.toString());
                    }
                } else if (inspect_command == "backtrace" || inspect_command == "bt") {
                    inspect_command = "bt";
                    inspect_print_info = new InspectPrintInfo(InspectPrintInfo::Type::backtrace);
                    auto binfo = dynamic_cast<InspectPrintInfo::BacktracePrintInfo*>(inspect_print_info->info);
                    binfo->num = instrumenter.getModule()->functions.size();
                    binfo->names.resize(binfo->num);
                    size_t i = 0;
                    for (const auto &f : instrumenter.getModule()->functions) {
                        if (f->imported()) {
                            binfo->names[i] = f->base.toString();
                        } else {
                            binfo->names[i] = f->name.toString();
                        }
                        binfo->funcname_map.emplace(binfo->names[i], i);
                        i++;
                    }
                } else {
                    std::printf(" Error: please enter valid command\n");
                    state = InspectState::commanding;
                    break;
                }
                state = InspectState::instrumenting;
                break;
            }
            case InspectState::instrumenting:
            {
                std::printf("(wabidb-inspect) Instrumenting ...\n");
                do_pre_instrument(instrumenter, inspect_func_name, inspect_line_num, inspect_command, *inspect_print_info);
                std::printf("(wabidb-inspect) Write instrumented file to: %s\n", options.extra["outfile"].c_str());
                instrumenter.writeBinary();
                state = InspectState::executing;
                break;
            }
            case InspectState::executing:
            {
                if (command.size() == 0) {
                    std::printf("(wabidb-inspect) No runtime command, you should deal with the instrumented wasm file yourself!\n");
                } else {
                    modify_runtime_command(command, options.extra["outfile"]);
                    std::printf("(wabidb-inspect) Executing with: \"%s\" ...\n", command.c_str());
                    if (run_runtime_command(command)) {
                        print_cache_file(*inspect_print_info, "__instr_cache.file");
                    }
                }
                state = InspectState::end;
                break;
            }
            case InspectState::end:
            {
                std::string next_cmd;
                std::printf("(wabidb-inspect) continue(c) | quit(q)\n > ");
                std::cin >> next_cmd;
                if (next_cmd.size() > 0) {
                    if (next_cmd[0] == 'c') {
                        delete inspect_print_info;
                        instrumenter.clear();
                        iresult = instrumenter.setConfig(config);
                        assert(iresult == InstrumentResult::success);
                        state = InspectState::positioning;
                    } else if (next_cmd[0] == 'q') {
                        delete inspect_print_info;
                        if_end = true;
                    } else {
                        state = InspectState::end;
                    }
                } else {
                    state = InspectState::end;
                }
                break;
            }
            default: assert(false);
        }
    }

    return 0;
}