(module
  (type (;0;) (func (param i32) (result i32)))
  (type (;1;) (func))
  (type (;2;) (func (param i32 i32 i32) (result i32)))
  (type (;3;) (func (param i32 i32 i32 i32) (result i32)))
  (type (;4;) (func (param i32 i32 i32 i32 i32 i64 i64 i32 i32) (result i32)))
  (type (;5;) (func (param i32)))
  (type (;6;) (func (param i32 i32) (result i32)))
  (type (;7;) (func (param i32 i32 i32)))
  (import "wasi_snapshot_preview1" "fd_write" (func (;0;) (type 3)))
  (import "wasi_snapshot_preview1" "path_open" (func (;1;) (type 4)))
  (import "wasi_snapshot_preview1" "fd_close" (func (;2;) (type 0)))
  (import "wasi_snapshot_preview1" "proc_exit" (func (;3;) (type 5)))
  (import "wasi_snapshot_preview1" "fd_prestat_get" (func (;4;) (type 6)))
  (import "wasi_snapshot_preview1" "fd_prestat_dir_name" (func (;5;) (type 2)))
  (func (;6;) (type 1))
  (func (;7;) (type 0) (param i32) (result i32)
    (local i32 i32)
    i32.const 1
    local.set 1
    local.get 0
    i32.const 1
    i32.sub
    local.tee 2
    i32.const 2
    i32.ge_u
    if (result i32)  ;; label = @1
      i32.const 0
      local.set 1
      loop  ;; label = @2
        local.get 2
        call 7
        local.get 1
        i32.add
        local.set 1
        local.get 0
        i32.const 3
        i32.sub
        local.set 2
        local.get 0
        i32.const 2
        i32.sub
        local.set 0
        local.get 2
        i32.const 1
        i32.gt_u
        br_if 0 (;@2;)
      end
      local.get 1
      i32.const 1
      i32.add
    else
      local.get 1
    end)
  (func (;8;) (type 1)
    call 10
    i32.const 65548
    i32.const 8
    call 7
    i32.store
    i32.const 65552
    i32.const 4
    i32.store
    i32.const 65536
    i32.const 10
    i32.const 65556
    call 9
    i32.const 65556
    i32.load
    i32.const 65548
    i32.const 1
    i32.const 65560
    call 0
    drop
    i32.const 65556
    i32.load
    call 2
    drop)
  (func (;9;) (type 7) (param i32 i32 i32)
    (local i32)
    i32.const 3072
    call 11
    local.tee 3
    i32.const 0
    i32.ne
    if  ;; label = @1
      local.get 3
      i32.const 2
      i32.mul
      call 3
    end
    i32.const 3072
    i32.load
    i32.const 1
    local.get 0
    local.get 1
    i32.const 1
    i64.const 66
    i64.const 66
    i32.const 0
    local.get 2
    call 1
    local.tee 3
    i32.const 0
    i32.ne
    if  ;; label = @1
      local.get 3
      i32.const 3
      i32.mul
      call 3
    end)
  (func (;10;) (type 1)
    i32.const 1
    memory.grow
    i32.const 65536
    i32.mul
    i32.const 0
    i32.const 10
    memory.init 0)
  (func (;11;) (type 0) (param i32) (result i32)
    (local i32 i32 i32)
    i32.const 3
    local.set 1
    block  ;; label = @1
      block  ;; label = @2
        loop  ;; label = @3
          local.get 1
          i32.const 3072
          call 4
          local.get 1
          i32.add
          local.tee 2
          local.get 1
          i32.ne
          br_if 1 (;@2;)
          i32.const 3076
          i32.load
          local.set 3
          local.get 1
          i32.const 3072
          local.get 3
          call 5
          i32.const 2
          i32.add
          local.tee 2
          i32.const 2
          i32.ne
          br_if 1 (;@2;)
          i32.const 3072
          i32.const 1024
          i32.const 1
          call 12
          i32.const 0
          i32.eq
          br_if 2 (;@1;)
          i32.const 1
          local.get 1
          i32.add
          local.set 1
          br 0 (;@3;)
        end
        unreachable
      end
      local.get 2
      return
    end
    local.get 0
    local.get 1
    i32.store
    local.get 2
    return)
  (func (;12;) (type 2) (param i32 i32 i32) (result i32)
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
        unreachable
      end
      local.get 4
      local.get 5
      i32.sub
      local.set 3
    end
    local.get 3)
  (memory (;0;) 1 2)
  (export "__wasm_call_ctors" (func 6))
  (export "__wasm_apply_data_relocs" (func 6))
  (export "fib" (func 7))
  (export "_start" (func 8))
  (export "memory" (memory 0))
  (data (;0;) "result.txt\00"))
