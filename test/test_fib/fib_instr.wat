(module
  (type (;0;) (func))
  (type (;1;) (func (param i32 i32 i32 i32) (result i32)))
  (type (;2;) (func (param i32) (result i32)))
  (type (;3;) (func (param i32 i32)))
  (import "wasi_snapshot_preview1" "fd_write" (func (;0;) (type 1)))
  (func (;1;) (type 0))
  (func (;2;) (type 2) (param i32) (result i32)
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
        call 3
        call 2
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
  (func (;3;) (type 0)
    i32.const 1
    global.get 0
    i32.add
    global.set 0)
  (func (;4;) (type 0)
    i32.const 4
    call 2
    drop
    global.get 0
    i32.const 1024
    call 5
    i32.const 512
    i32.const 1024
    i32.store
    i32.const 516
    i32.const 8
    i32.store
    i32.const 1
    i32.const 512
    i32.const 1
    i32.const 24
    call 0
    drop)
  (func (;5;) (type 3) (param i32 i32)
    (local i32 i32 i32 i32 i32 i32)
    local.get 0
    if  ;; label = @1
      i32.const 12800
      local.set 4
      loop  ;; label = @2
        local.get 4
        i32.const 6
        i32.add
        local.get 2
        i32.add
        local.get 0
        local.get 0
        i32.const 10
        i32.div_s
        local.tee 3
        i32.const 10
        i32.mul
        i32.sub
        i32.const 48
        i32.add
        i32.store8
        local.get 2
        i32.const 1
        i32.add
        local.set 2
        local.get 0
        i32.const 9
        i32.add
        local.set 6
        local.get 3
        local.set 0
        local.get 6
        i32.const 19
        i32.ge_u
        br_if 0 (;@2;)
      end
      loop  ;; label = @2
        local.get 1
        local.get 2
        i32.const 1
        i32.sub
        local.tee 0
        local.get 4
        i32.const 6
        i32.add
        i32.add
        i32.load8_u
        i32.store8
        local.get 1
        i32.const 1
        i32.add
        local.set 1
        local.get 2
        i32.const 1
        i32.gt_s
        local.set 7
        local.get 0
        local.set 2
        local.get 7
        br_if 0 (;@2;)
      end
    end)
  (memory (;0;) 256 256)
  (global (;0;) (mut i32) (i32.const 0))
  (export "__wasm_call_ctors" (func 1))
  (export "__wasm_apply_data_relocs" (func 1))
  (export "fib" (func 2))
  (export "_start" (func 4))
  (export "memory" (memory 0)))
