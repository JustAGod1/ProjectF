#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <deque>
#include <concepts>

#include "utils/utils.hpp"
#include "interpreter/special_forms.hpp"

class Interpreter;
class InterpreterNode;

template <typename R>
class EvaluationResultThrowed;

enum class EvaluationResultType { OK, BREAK, RETURN };
template <typename R>
class EvaluationResult {
private:

  template <typename FriendR>
  friend class EvaluationResult;
  using Marker = std::true_type;

  EvaluationResultType type;
  union {
    R value;
    std::shared_ptr<InterpreterNode> return_value;
  };

  EvaluationResult(std::shared_ptr<InterpreterNode> value,
      EvaluationResultType type) : return_value(std::move(value)), type(type) {}
public:

  class EvaluationResultThrowed;

  friend class EvaluationResultThrowed; 

  class EvaluationResultThrowed {
    friend EvaluationResult;
    std::shared_ptr<InterpreterNode> return_value;
    EvaluationResultType type;

    EvaluationResultThrowed(std::shared_ptr<InterpreterNode> return_value, EvaluationResultType type) 
      : return_value(return_value)
      , type(type) {}

  public:
    template<typename V>
    operator EvaluationResult<V>() {
      return EvaluationResult<V>{return_value, type};
    }
  };

  template<typename V>
  requires (requires(V v) { R{v};} )
  EvaluationResult(V&& value)  : value(std::move(value)), type(EvaluationResultType::OK) {}

  template<typename V>
  requires (requires(V v) { R{v};} )
  EvaluationResult(EvaluationResult<V>&& child) : value(child.value), type(child.type) {}

  static EvaluationResult<R> ok(R v) { return EvaluationResult{std::move(v)}; }
  static EvaluationResult<R> ret(R v) { return EvaluationResult{std::move(v), EvaluationResultType::RETURN}; }
  static EvaluationResult<R> bre() { return EvaluationResult{nullptr, EvaluationResultType::BREAK}; }

  EvaluationResultType get_type() { return type; }
  bool is_ret() { return type == EvaluationResultType::RETURN; }
  bool is_bre() { return type == EvaluationResultType::BREAK; }
  bool is_ok() { return type == EvaluationResultType::OK; }

  auto to_throw() const {
    assert(type != EvaluationResultType::OK, "shouldn't be ok");
    return EvaluationResultThrowed{return_value, type};
  }
  
  R& get_value() { 
    assert(type == EvaluationResultType::OK, "should be ok");
    return value;
  }

  R& get_return() { 
    bool kek;
    assert(type == EvaluationResultType::RETURN, "should be return");
    return value;
  }

  ~EvaluationResult() {
    if (type == EvaluationResultType::OK) {
      value.~R();
    } else {
      return_value.~shared_ptr<InterpreterNode>();
    }
  }
};




#define get_or_ret(result) ({\
    auto r = (result);\
    if (r.get_type() != EvaluationResultType::OK) {\
      return r.to_throw(); \
    }\
    r.get_value();\
    })


class InterpreterNode {
private:
  friend class Interpreter;

public:
  virtual ~InterpreterNode() = default;

  virtual EvaluationResult<std::shared_ptr<InterpreterNode>> evaluate(
      std::shared_ptr<InterpreterNode> self,
      Interpreter& Interpreter
    ) const = 0;

  virtual void print(std::ostream& out, int indent = 0) const = 0;
};

using InterpreterNodePtr = std::shared_ptr<InterpreterNode>;

class CallableNode : public InterpreterNode { };


class Context {
private:
  class ContextLayer {
  private:

    template<typename ... Bases>
    struct overload : Bases ...
    {
        using is_transparent = void;
        using Bases::operator() ... ;
    };

    using TransparentStringHash = overload<
        std::hash<std::string>,
        std::hash<std::string_view>
    >;

    std::unordered_map<std::string, std::shared_ptr<InterpreterNode>, TransparentStringHash, std::equal_to<>> variables;

  public:
    std::shared_ptr<InterpreterNode> get(std::string_view name);
    void set(std::string_view name, std::shared_ptr<InterpreterNode>);
  };

private:
  std::deque<ContextLayer> layers;
  void pop_layer();
  ContextLayer& root_layer;
public:

  class ContextLayerToken {
  private:
    friend Context;
    Context& context;
    explicit ContextLayerToken(Context& context) : context(context) {}
  public:
    ~ContextLayerToken() { context.pop_layer(); }
  };
  friend ContextLayerToken;

  std::shared_ptr<InterpreterNode> get(std::string_view name);
  void set(std::string_view name, std::shared_ptr<InterpreterNode> value);
  ContextLayerToken create_layer();

  void set_in_root(std::string_view name, std::shared_ptr<InterpreterNode> value);

  Context();

};

class InterpreterStack {
private:
  std::deque<std::shared_ptr<InterpreterNode>> content;
public:

  void push(std::shared_ptr<InterpreterNode> value);
  std::shared_ptr<InterpreterNode> pop();
  std::optional<std::shared_ptr<InterpreterNode>> pop_or_null();
  void assert_empty();

};

class Interpreter {
private:
  Context context;
  InterpreterStack stack;
public:
  explicit Interpreter() {
    register_special_forms(context);
  }

  Context& get_context() { return context; }
  InterpreterStack& get_stack() { return stack; }
};

