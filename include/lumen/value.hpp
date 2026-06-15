#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace lumen {

// ── Forward declarations ──────────────────────────────────────────────────────
struct ObjString;
struct ObjFunction;
struct ObjClosure;
struct ObjUpvalue;
struct ObjNative;
struct Chunk;

// ── Base heap object ──────────────────────────────────────────────────────────
struct Obj {
    enum class Type { String, Function, Closure, Upvalue, Native };
    Type objType;
    virtual ~Obj() = default;
};

// ── Value (tagged union) ──────────────────────────────────────────────────────
// Stack-allocated.  Heap objects referenced via shared_ptr inside Obj*.
// Using shared_ptr for simplicity — Phase 2 upgrades to a mark-sweep GC.
struct Value {
    enum class Tag : uint8_t { Nil, Bool, Number, Obj } tag{Tag::Nil};
    union { bool b{false}; double n; };
    std::shared_ptr<Obj> obj; // valid when tag == Obj

    // ── Predicates ────────────────────────────────────────────────────────────
    bool isNil()      const noexcept { return tag == Tag::Nil; }
    bool isBool()     const noexcept { return tag == Tag::Bool; }
    bool isNumber()   const noexcept { return tag == Tag::Number; }
    bool isObj()      const noexcept { return tag == Tag::Obj; }
    bool isString()   const noexcept;
    bool isFunction() const noexcept;
    bool isClosure()  const noexcept;
    bool isNative()   const noexcept;
    bool isTruthy()   const noexcept;

    // ── Accessors (unchecked) ─────────────────────────────────────────────────
    bool       asBool()     const noexcept { return b; }
    double     asNumber()   const noexcept { return n; }
    ObjString*   asString()   const noexcept;
    ObjFunction* asFunction() const noexcept;
    ObjClosure*  asClosure()  const noexcept;
    ObjNative*   asNative()   const noexcept;

    // ── Factories ─────────────────────────────────────────────────────────────
    static Value nil()              { return {}; }
    static Value boolean(bool b_)   { Value v; v.tag = Tag::Bool; v.b = b_; return v; }
    static Value number(double n_)  { Value v; v.tag = Tag::Number; v.n = n_; return v; }
    static Value fromObj(std::shared_ptr<Obj> o) {
        Value v; v.tag = Tag::Obj; v.obj = std::move(o); return v;
    }

    // ── Operators ─────────────────────────────────────────────────────────────
    bool operator==(const Value& o) const noexcept;
    std::string toString() const;
};

// ── Concrete heap objects ─────────────────────────────────────────────────────

struct ObjString : Obj {
    std::string chars;
    explicit ObjString(std::string s) : chars(std::move(s)) { objType = Obj::Type::String; }

    static Value make(std::string s) {
        return Value::fromObj(std::make_shared<ObjString>(std::move(s)));
    }
};

struct ObjFunction : Obj {
    std::string              name;
    int                      arity{0};
    int                      upvalue_count{0};
    std::shared_ptr<Chunk>   chunk;

    ObjFunction();                 // defined in compiler.cpp (needs Chunk)
    ~ObjFunction() override = default;

    static Value make(std::string name_, int arity_);
};

// Upvalue: bridges stack-allocated variables and heap-allocated closures.
// While the captured local lives on the stack, `location` points into the
// VM's value stack.  When the stack frame is unwound, the upvalue is "closed"
// — the value is copied to `closed` and `location` is redirected to it.
struct ObjUpvalue : Obj {
    Value*                          location; // points into VM stack OR &closed
    Value                           closed;   // storage after close()
    std::shared_ptr<ObjUpvalue>     next;     // intrusive list of open upvalues

    explicit ObjUpvalue(Value* loc) : location(loc) { objType = Obj::Type::Upvalue; }

    void close() { closed = *location; location = &closed; }
};

struct ObjClosure : Obj {
    std::shared_ptr<ObjFunction>              fn;
    std::vector<std::shared_ptr<ObjUpvalue>>  upvalues;

    explicit ObjClosure(std::shared_ptr<ObjFunction> f)
        : fn(std::move(f)) {
        objType = Obj::Type::Closure;
        upvalues.resize(fn->upvalue_count);
    }

    static Value make(std::shared_ptr<ObjFunction> f) {
        return Value::fromObj(std::make_shared<ObjClosure>(std::move(f)));
    }
};

using NativeFn = std::function<Value(int argc, Value* argv)>;
struct ObjNative : Obj {
    NativeFn fn;
    std::string name;
    ObjNative(std::string n, NativeFn f) : fn(std::move(f)), name(std::move(n)) {
        objType = Obj::Type::Native;
    }
    static Value make(std::string n, NativeFn f) {
        return Value::fromObj(std::make_shared<ObjNative>(std::move(n), std::move(f)));
    }
};

// ── Inline implementations ────────────────────────────────────────────────────
inline bool Value::isString()   const noexcept { return isObj() && obj->objType == Obj::Type::String;   }
inline bool Value::isFunction() const noexcept { return isObj() && obj->objType == Obj::Type::Function; }
inline bool Value::isClosure()  const noexcept { return isObj() && obj->objType == Obj::Type::Closure;  }
inline bool Value::isNative()   const noexcept { return isObj() && obj->objType == Obj::Type::Native;   }

inline ObjString*   Value::asString()   const noexcept { return static_cast<ObjString*>  (obj.get()); }
inline ObjFunction* Value::asFunction() const noexcept { return static_cast<ObjFunction*>(obj.get()); }
inline ObjClosure*  Value::asClosure()  const noexcept { return static_cast<ObjClosure*> (obj.get()); }
inline ObjNative*   Value::asNative()   const noexcept { return static_cast<ObjNative*>  (obj.get()); }

inline bool Value::isTruthy() const noexcept {
    if (isNil())  return false;
    if (isBool()) return b;
    return true;
}

} // namespace lumen
