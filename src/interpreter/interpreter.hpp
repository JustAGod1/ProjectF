#pragma once

#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <ostream>
#include <sstream>
#include <exception>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <stdarg.h>
#include <fmt/base.h>
#include <fmt/format.h>
#include <utility>

#include "utils/utils.hpp"
#include "interpreter/special_forms.hpp"
#include "parser/node_location.hpp"

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
    assert_unverbose(type != EvaluationResultType::OK, "shouldn't be ok");
    return EvaluationResultThrowed{return_value, type};
  }
  
  R& get_value() { 
    assert_unverbose(type == EvaluationResultType::OK, "should be ok");
    return value;
  }

  R& get_return() { 
    bool kek;
    assert_unverbose(type == EvaluationResultType::RETURN, "should be return");
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
  const std::optional<NodeLocation> location;
  virtual ~InterpreterNode() = default;

  explicit InterpreterNode(std::optional<NodeLocation> location) : location(location) {}

  virtual EvaluationResult<std::shared_ptr<InterpreterNode>> evaluate(
      std::shared_ptr<InterpreterNode> self,
      Interpreter& Interpreter
    ) const = 0;

  virtual void print(std::ostream& out, int indent = 0) const = 0;

  std::string to_string() const {
    std::stringstream ss;
    print(ss);
    return ss.str();
  }
};

using InterpreterNodePtr = std::shared_ptr<InterpreterNode>;

class CallableNode : public InterpreterNode {
public:
  CallableNode() : InterpreterNode(std::nullopt) {}
};


class Context {
private:
  class ContextLayer {
  private:
    friend Context;

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
  void pop_layer(std::optional<std::unordered_set<std::string>>);
  ContextLayer& root_layer;
public:

  class ContextLayerToken {
  private:
    friend Context;
    Context& context;
    std::optional<std::unordered_set<std::string>> exceptions;
    explicit ContextLayerToken(Context& context) : context(context) {}
    explicit ContextLayerToken(Context& context, std::unordered_set<std::string> exceptions) 
      : context(context)
      , exceptions(std::move(exceptions))  {}
  public:
    ~ContextLayerToken() { context.pop_layer(std::move(exceptions)); }
  };
  friend ContextLayerToken;

  std::shared_ptr<InterpreterNode> get(std::string_view name);
  void set(std::string_view name, std::shared_ptr<InterpreterNode> value);
  ContextLayerToken create_layer();
  ContextLayerToken create_layer(std::unordered_set<std::string> exceptions);
  ContextLayerToken create_layer(std::initializer_list<std::string> exceptions) { 
    return create_layer(std::unordered_set(exceptions));
  }

  void set_in_root(std::string_view name, std::shared_ptr<InterpreterNode> value);

  Context();

};

class InterpreterStack {
private:
  std::deque<std::shared_ptr<InterpreterNode>> content;
public:

  void push(std::shared_ptr<InterpreterNode> value);
  int available() { return content.size(); }
  std::shared_ptr<InterpreterNode> pop(Interpreter& interpreter, InterpreterNodePtr node);
  std::optional<std::shared_ptr<InterpreterNode>> pop_or_null();
  bool is_empty();

};
class EvaluatonException;

class Interpreter {
private:
  Context context;
  InterpreterStack stack;
  const std::string_view source;

  friend class EvaluatonException;

  [[noreturn]]
  void exit_fatal(std::string message, const InterpreterNode& node) const {
      fmt::println("Condition failed: {}", message);
      if (node.location.has_value()) {
        node.location.value().print_line_error(source, std::cout);
        std::cout << std::endl;
      } else {
        fmt::println("Given node {} has no location", node.to_string());
      }

      exit(1);
  }

public:
  explicit Interpreter(std::string_view source) : source(source) {
    register_special_forms(context);
  }

  template<typename LikeBool, typename... Args>
  void assert_verbose(const InterpreterNodePtr& node, LikeBool condition, const char* fmt, Args&&... args) const {
    assert_verbose(*node.get(), condition, fmt, args...);
  }
  template<typename LikeBool, typename... Args>
  void assert_verbose(const InterpreterNode& node, LikeBool condition, const char* fmt, Args&&... args) const {
    if (!condition) {
      exit_fatal(fmt::format(fmt::runtime(fmt), args...), node);
    }
  }

  Context& get_context() { return context; }
  InterpreterStack& get_stack() { return stack; }
};

class EvaluatonException : public std::exception {
private:
  std::string message;
public:
  EvaluatonException(std::string message) : message(message) {}

  template<typename... Args>
  EvaluatonException(const char* fmt, Args&&... args) : message(fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...)) {}

  [[noreturn]]
  void exit_fatal(const Interpreter& interpreter, InterpreterNodePtr node) {
    interpreter.exit_fatal(std::move(message), *node.get());
  }

  template<typename... Args>
  static void throw_if_false(bool condition, const char* fmt, Args&&... args) {
    if (!condition) {
      throw EvaluatonException{fmt, std::forward<Args>(args)...};
    }
  }
};

