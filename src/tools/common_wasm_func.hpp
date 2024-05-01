#ifndef common_wasm_func_h
#define common_wasm_func_h
#include <map>
#include <optional>
#include <string>

namespace wasm_instrument {

class CommonWasmBuilder final {
public:
    CommonWasmBuilder() = default;
    std::optional<const std::string> getWasmFunction(const std::string &name) const {
        auto iter = this->wasm_func_map_.find(name);
        if (iter != this->wasm_func_map_.end()) {
            return iter->second;
        } else {
            return std::nullopt;
        }
    }
    std::optional<const std::string> getWasiName(const std::string &wasi) const {
        auto iter = this->wasi_name_map_.find(wasi);
        if (iter != this->wasi_name_map_.end()) {
            return iter->second;
        } else {
            return std::nullopt;
        }
    }
    void updateWasiName(const std::string &wasi, const std::string &name) {
        const auto original_name = this->wasi_name_map_.find(wasi);
        if (original_name == this->wasi_name_map_.end()) {
            this->wasi_name_map_.emplace(wasi, name);
        } else {
            for (auto &[_, v] : this->wasm_func_map_) {
                size_t idx = 0;
                while (((idx = v.find(original_name->second, idx)) != std::string::npos)) {
                    v.replace(idx, original_name->second.size(), name);
                    idx += name.size();
                }
            }
            this->wasi_name_map_[wasi] = name;
        }
        
    }

private:
    std::map<std::string, std::string> wasi_name_map_ {
        {"fd_prestat_get", "fd_prestat_get"},
        {"fd_prestat_dir_name", "fd_prestat_dir_name"},
        {"path_open", "path_open"},
        {"fd_write", "fd_write"},
        {"fd_close", "fd_close"},
        {"proc_exit", "proc_exit"},
    };
    std::map<std::string, std::string> wasm_func_map_ {
        {"__instr_memcmp", R"((func $__instr_memcmp (param i32 i32 i32) (result i32)
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
  ))"},
        {"__instr_get_cwd_fd", R"((func $__instr_get_cwd_fd (param $dot_addr i32) (param $fd_addr i32) (result i32)
    (local $dirfd i32)
    (local $errno i32)
    (local $dir_name_len i32)
    i32.const 3
    local.set $dirfd
    (block $b1 ;; label = @1
      (block $b2 ;; label = @2
        (loop $loop ;; label = @3
          local.get $dirfd
          local.get $fd_addr
          call $fd_prestat_get
          local.tee $errno
          i32.const 0
          i32.ne
          br_if $b2 (;@2;)
          local.get $fd_addr
          i32.const 4
          i32.add
          i32.load
          local.set $dir_name_len
          local.get $dirfd
          local.get $fd_addr
          local.get $dir_name_len
          call $fd_prestat_dir_name
          local.tee $errno
          i32.const 0
          i32.ne
          br_if $b2 (;@2;)
          local.get $fd_addr
          local.get $dot_addr
          i32.const 1
          call $__instr_memcmp
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
  ))"},
        //(i32 cwd_fd i32 path_addr, i32 path_len, i32 fd_addr) -> (i32 errno)
        {"__instr_fopen_rw", R"((func $__instr_fopen_rw (param i32 i32 i32 i32) (result i32)
    local.get 0
    i32.const 1
    local.get 1
    local.get 2
    i32.const 9
    i64.const 66
    i64.const 66
    i32.const 0
    local.get 3
    call $path_open
  ))"},
    };
};

}

#endif