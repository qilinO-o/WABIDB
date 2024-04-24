#include "instrumenter.hpp"

using namespace wasm_instrument;

/*
* test_path_open doc:
* 1. read in a side module of a simple fibonacci function
* 2. add WASI function path_open(), fd_write(), fd_close()
* 3. add memory section
* 4. add function as _start for running the test in standalone runtimes
*    the _start function calls fib(x) then write its result to file "./result.txt"
*/
int main() {
    // run the test under build directory
    std::string relative_path = "../test/test_path_open/";
    
    InstrumentConfig config;
    // same to the wasm file in test_fib
    config.filename = relative_path + "fib.wasm";
    config.targetname = relative_path + "fib_instr.wasm";

    // set the input / output file name and open module
    Instrumenter instrumenter;
    InstrumentResult result = instrumenter.setConfig(config);
    assert(result == InstrumentResult::success);
    
    // create a function type for (i32, i32, i32, i32) -> (i32)
    // import WASI function fd_write()
    BinaryenType iiii[4] = {BinaryenTypeInt32(), BinaryenTypeInt32(), BinaryenTypeInt32(), BinaryenTypeInt32()};
    BinaryenType fdwrite_params = BinaryenTypeCreate(iiii, 4);
    instrumenter.addImportFunction("fd_write", "wasi_snapshot_preview1", "fd_write", 
                                    fdwrite_params, BinaryenTypeInt32());

    // create a function type for (i32 i32 i32 i32 i32 i64 i64 i32 i32) -> (i32)
    // import WASI function path_open()
    BinaryenType i9[9] = {BinaryenTypeInt32(), BinaryenTypeInt32(), BinaryenTypeInt32(), BinaryenTypeInt32(), 
                        BinaryenTypeInt32(), BinaryenTypeInt64(), BinaryenTypeInt64(), BinaryenTypeInt32(), 
                        BinaryenTypeInt32()};
    BinaryenType pathopen_params = BinaryenTypeCreate(i9, 9);
    instrumenter.addImportFunction("path_open", "wasi_snapshot_preview1", "path_open", 
                                    pathopen_params, BinaryenTypeInt32());

    // create a function type for (i32) -> (i32)
    // import WASI function fd_close()
    instrumenter.addImportFunction("fd_close", "wasi_snapshot_preview1", "fd_close", 
                                    BinaryenTypeInt32(), BinaryenTypeInt32());

    // create a function type for (i32) -> ()
    // import WASI function proc_exit()
    instrumenter.addImportFunction("proc_exit", "wasi_snapshot_preview1", "proc_exit",
                                    BinaryenTypeInt32(), BinaryenTypeNone());

    // create a function type for (i32, i32) -> (i32)
    // import WASI function fd_prestat_get()
    BinaryenType ii[2] = {BinaryenTypeInt32(), BinaryenTypeInt32()};
    BinaryenType fd_prestat_get_params = BinaryenTypeCreate(ii, 2);
    instrumenter.addImportFunction("fd_prestat_get", "wasi_snapshot_preview1", "fd_prestat_get", 
                                    fd_prestat_get_params, BinaryenTypeInt32());

    // create a function type for (i32, i32, i32) -> (i32)
    // import WASI function fd_prestat_dir_name()
    BinaryenType iii[3] = {BinaryenTypeInt32(), BinaryenTypeInt32(), BinaryenTypeInt32()};
    BinaryenType fd_prestat_dir_name_params = BinaryenTypeCreate(iii, 3);
    instrumenter.addImportFunction("fd_prestat_dir_name", "wasi_snapshot_preview1", "fd_prestat_dir_name", 
                                    fd_prestat_dir_name_params, BinaryenTypeInt32());

    // add memory for further functions
    auto ret1 = instrumenter.addMemory("mem", false, 1, 2);
    assert(ret1 != nullptr);

    // add file name in a passive data segment
    // later load the segment to a new page with memory.grow and memory.init
    auto ret2 = instrumenter.addPassiveDateSegment(".instr_filename", "file.txt\00.\00", 11);
    assert(ret2 != nullptr);
    
    // add a function which calls function fib(x = 9) and do write job
    // add a function to simplify path_open call with call of:
    //     dir_fd = AT_FDCWD(-100); dirflags = 0; oflags = 1(create); 
    //     fs_rights = 2 | 64(read & write); fdflags = 0
    // func rwopen(i32 path_addr, i32 path_len, i32 fd_addr)
    // add a function to load literal filename from a passive data segment to memory
    auto export_func_fib = instrumenter.getExport("fib");
    auto func_fib = instrumenter.getFunction(export_func_fib->value.toString().c_str());
    auto internal_func_fib_name = func_fib->name.toString();
    instrumenter.addFunctions({"do_test", "fopen_rw", "load_filename", "get_cwd_fd", "memcmp",}, 
    {
        "(func $do_test\n"
            "(local $errno i32)\n"
            "call $load_filename\n"
            "i32.const " + std::to_string(65536 + 48) + "\n" //result addr (icvec_array pointer)
            "i32.const 9\n"
            "call $" + internal_func_fib_name + "\n"
            "i32.store\n"

            "i32.const " + std::to_string(65536 + 12) + "\n" //ciovec result addr
            "i32.const " + std::to_string(65536 + 48) + "\n"
            "i32.store\n"
            "i32.const " + std::to_string(65536 + 12 + 4) + "\n" //ciovec result len addr
            "i32.const 4\n"
            "i32.store\n"
            "i32.const 65536\n" //filename addr
            "i32.const 8\n"
            "i32.const " + std::to_string(65536 + 12 + 8) + "\n" //fd addr
            "call $fopen_rw\n"
            "i32.const " + std::to_string(65536 + 12 + 8) + "\n"
            "i32.load\n"
            "i32.const " + std::to_string(65536 + 12) + "\n"
            "i32.const 1\n"
            "i32.const " + std::to_string(65536 + 12 + 12) + "\n" //written len addr
            "call $fd_write\n"
            "local.tee $errno\n"
            "i32.const 0\n"
            "i32.ne\n"
            "if\n"
            "local.get $errno\n"
            "call $proc_exit\n"
            "end\n"
            "i32.const " + std::to_string(65536 + 12 + 8) + "\n"
            "i32.load\n"
            "call $fd_close\n"
            "drop\n)",
        "(func $fopen_rw (param i32 i32 i32)\n"
            "(local $errno i32)\n"
            "i32.const 3072\n"
            "call $get_cwd_fd\n"
            "local.tee $errno\n"
            "i32.const 0\n"
            "i32.ne\n"
            "if\n"
            "local.get $errno\n"
            "call $proc_exit\n"
            "end\n"
            "i32.const 3072\n"
            "i32.load\n"
            // "i32.const -100\n"
            "i32.const 1\n"
            "local.get 0\n"
            "local.get 1\n"
            "i32.const 9\n"
            "i64.const 66\n"
            "i64.const 66\n"
            "i32.const 0\n"
            "local.get 2\n"
            "call $path_open\n"
            "local.tee $errno\n"
            "i32.const 0\n"
            "i32.ne\n"
            "if\n"
            "local.get $errno\n"
            "call $proc_exit\n"
            "end\n)",
        "(func $load_filename\n"
            "i32.const 1\n"
            "memory.grow\n"
            "i32.const 65536\n"
            "i32.mul\n"
            "i32.const 0\n"
            "i32.const 10\n"
            "memory.init $.instr_filename\n)",
        R"((func $get_cwd_fd (param $fd_addr i32) (result i32)
    (local $dirfd i32)
    (local $errno i32)
    (local $dir_name_len i32)
    i32.const 3
    local.set $dirfd
    (block $b1 ;; label = @1
      (block $b2 ;; label = @2
        (loop $loop ;; label = @3
          local.get $dirfd
          i32.const 3072
          call $fd_prestat_get
          local.tee $errno
          i32.const 0
          i32.ne
          br_if $b2 (;@2;)
          i32.const 3076
          i32.load
          local.set $dir_name_len
          local.get $dirfd
          i32.const 3072
          local.get $dir_name_len
          call $fd_prestat_dir_name
          local.tee $errno
          i32.const 0
          i32.ne
          br_if $b2 (;@2;)
          i32.const 3072
          i32.const 65545
          i32.const 1
          call $memcmp
          i32.const 0
          i32.eq
          br_if $b1 (;@1;)
          i32.const 1
          local.get $dirfd
          i32.add
          local.set $dirfd
          br $loop
        )
      )
      local.get $errno
      return
    )
    local.get $fd_addr
    local.get $dirfd
    i32.store
    local.get $errno
    return
  ))",
        R"((func $memcmp (param i32 i32 i32) (result i32)
    (local i32 i32 i32)
    i32.const 0
    local.set 3
    block  ;; label = @1
      local.get 2
      i32.eqz
      br_if 0 (;@1;)
      block  ;; label = @2
        loop  ;; label = @3
          local.get 0
          i32.load8_u
          local.tee 4
          local.get 1
          i32.load8_u
          local.tee 5
          i32.ne
          br_if 1 (;@2;)
          local.get 1
          i32.const 1
          i32.add
          local.set 1
          local.get 0
          i32.const 1
          i32.add
          local.set 0
          local.get 2
          i32.const -1
          i32.add
          local.tee 2
          br_if 0 (;@3;)
          br 2 (;@1;)
        end
      end
      local.get 4
      local.get 5
      i32.sub
      local.set 3
    end
    local.get 3
  ))",
    });
    
    // set function do_test() as _start
    auto ret3 = instrumenter.addExport(wasm::ModuleItemKind::Function, "do_test", "_start");
    assert(ret3 != nullptr);
    
    // a module with WASI apis must export memory by default
    ret3 = instrumenter.addExport(wasm::ModuleItemKind::Memory, "mem", "memory");
    assert(ret3 != nullptr);
    
    // write back the instrumented module
    result = instrumenter.writeBinary();
    assert(result == InstrumentResult::success);
    return 0;
}