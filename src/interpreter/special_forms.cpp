#include "interpreter/special_forms.hpp"

#include <algorithm>
#include <deque>
#include <iostream>
#include <tuple>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <type_traits>
#include <utility>
#include <vector>
#include "fmt/base.h"
#include "fmt/format.h"
#include "interpreter/interpreter.hpp"
#include "parser/parser_structs.hpp"
#include "utils/utils.hpp"

static std::deque<InterpreterNodePtr>& expect_n_args(std::deque<InterpreterNodePtr>& args, int n) {
  EvaluatonException::throw_if_false(args.size() == n,
      "Expected {} arguments but got {}",
      n,
      args.size()
    );
  return args;
}

class SpecialFormNode : public CallableNode {
public:
  virtual std::string description() const = 0;
  void print(std::ostream& out, int indent = 0) const override {
    out << "Special Form " << description();
  }
};

class QuoteSpecialNode : public SpecialFormNode {
public:
  std::string description() const override { return "quote"; }

  EvaluationResult<InterpreterNodePtr> evaluate(
      NotNullSharedPtr<InterpreterNode> self,
      Interpreter& interpreter,
      std::deque<NotNullSharedPtr<InterpreterNode>> args
    ) const override {
    return expect_n_args(args, 1)[0];
  }
};

static EvaluationResult<NotNullSharedPtr<InterpreterNode>> eval(Interpreter& interpreter, NotNullSharedPtr<InterpreterNode> node) {
  return node->evaluate(node, interpreter, std::deque<InterpreterNodePtr>{});
}

template <class Derive>
static EvaluationResult<NotNullSharedPtr<Derive>> eval_to_type(Interpreter& interpreter, NotNullSharedPtr<InterpreterNode> node) {
  auto raw = get_or_ret(eval(interpreter, node));
  auto downcasted = (raw).dyn_cast<Derive>();
  if (!downcasted) {
    interpreter.assert_verbose(node.get(),
        downcasted,
        "Expected to be evaluated to {} but evaluated to {}",
        typeid(Derive).name(),
        typeid(raw.get()).name());
  }
  return *downcasted;
}

static EvaluationResult<bool> eval_to_bool(Interpreter& interpreter, NotNullSharedPtr<InterpreterNode> node) {
  auto literal = get_or_ret(eval_to_type<Literal>(interpreter, node));
  if (literal->type != Literal::Type::BOOLEAN) {
    std::ostringstream sstream{};
    node->print(sstream);
    interpreter.assert_verbose(node.get(),
        literal->type != Literal::Type::BOOLEAN,
        "{} expected to be evaluated to BOOLEAN but evaluated to {}",
        sstream.str().c_str(),
        literal->type_name());
  }

  return literal->boolValue;
}

template <class Derive>
NotNullSharedPtr<Derive> force_downcast(const Interpreter& interpreter, InterpreterNodePtr base) {
  auto downcasted = base.dyn_cast<Derive>();
  interpreter.assert_verbose(base, downcasted, "Failed to cast {} to {}", typeid(base.get()).name(), typeid(Derive).name());
  return *downcasted;
}

class SetqSpecialNode : public SpecialFormNode {
public:
  std::string description() const override { return "setq"; }

  EvaluationResult<InterpreterNodePtr> evaluate(
      NotNullSharedPtr<InterpreterNode> self,
      Interpreter& interpreter,
      std::deque<NotNullSharedPtr<InterpreterNode>> args
    ) const override {
    expect_n_args(args, 2);

    auto maybe_atom = force_downcast<Atom>(interpreter, args[0]);
    
    auto value = get_or_ret(eval(interpreter, args[1]));

    interpreter.get_context().set(maybe_atom->identifier->name, value);

    return null_node();
  }
};

class LambdaValueNode : public CallableNode {
private:
  std::vector<std::string> arg_names;
  InterpreterNodePtr body;
public:
  explicit LambdaValueNode(std::vector<std::string> arg_names, InterpreterNodePtr body) : arg_names(arg_names), body(body) {}


  EvaluationResult<InterpreterNodePtr> evaluate(
      NotNullSharedPtr<InterpreterNode> self,
      Interpreter& interpreter,
      std::deque<NotNullSharedPtr<InterpreterNode>> args
    ) const override {
    std::vector<InterpreterNodePtr> evaluated;
    expect_n_args(args, arg_names.size());
    for (int i = 0; i < args.size(); i++) {
      evaluated.emplace_back(get_or_ret(eval(interpreter, args[i])));
    }

    auto tok = interpreter.get_context().create_layer();
    for (int i = 0; i < args.size(); i++) {
      interpreter.get_context().set(arg_names[i], evaluated[i]);
    }
    
    auto r = eval(interpreter, body);

    if (r.is_ret()) {
      return r.get_return();
    }

    return r;
  }

  void print(std::ostream& out, int indent = 0) const override {
    out << "LambdaValue ";
    for (auto i : arg_names) {
      std::cout << "(" << i << ") ";
    }
    body->print(out, indent);
  }

};


class LambdaSpecialNode : public SpecialFormNode {
public:
  std::string description() const override { return "lambda"; }

  EvaluationResult<InterpreterNodePtr> evaluate(
      NotNullSharedPtr<InterpreterNode> self,
      Interpreter& interpreter,
      std::deque<NotNullSharedPtr<InterpreterNode>> args
    ) const override {
    expect_n_args(args, 2);

    auto maybe_list = force_downcast<List>(interpreter, args[0]);

    std::vector<std::string> names;
    std::ranges::transform(maybe_list->elements, std::back_inserter(names), [&](auto &v) {
        NotNullSharedPtr<Atom> vd = force_downcast<Atom>(interpreter, v);
        return vd->identifier->name;
      });

    auto body = args[1];
    
    return make_nn_shared<LambdaValueNode>(names, body);
  }
};

class FuncSpecialNode : public SpecialFormNode {
public:
  std::string description() const override { return "func"; }

  EvaluationResult<InterpreterNodePtr> evaluate(
      NotNullSharedPtr<InterpreterNode> self,
      Interpreter& interpreter,
      std::deque<NotNullSharedPtr<InterpreterNode>> args
    ) const override {
    expect_n_args(args, 3);

    auto maybe_atom = force_downcast<Atom>(interpreter, args[0]);

    auto maybe_list = force_downcast<List>(interpreter, args[1]);

    std::vector<std::string> names;
    std::ranges::transform(maybe_list->elements, std::back_inserter(names), [&](auto &v) {
        NotNullSharedPtr<Atom> vd = force_downcast<Atom>(interpreter, v);
        return vd->identifier->name;
      });
    
    auto body = args[2];

    auto v = make_nn_shared<LambdaValueNode>(names, body);

    interpreter.get_context().set(maybe_atom->identifier->name, v);

    return null_node();
  }
};

class CondSpecialNode : public SpecialFormNode {
public:
  std::string description() const override { return "cond"; }

  EvaluationResult<InterpreterNodePtr> evaluate(
      NotNullSharedPtr<InterpreterNode> self,
      Interpreter& interpreter,
      std::deque<NotNullSharedPtr<InterpreterNode>> args
    ) const override {
    EvaluatonException::throw_if_false(args.size() >= 2,
        "cond expects at least 2 args got {}",
        args.size());
    EvaluatonException::throw_if_false(args.size() <= 3,
      "cond expects at most 3 args got {}",
      args.size()
        );

    InterpreterNodePtr true_branch = args[1];
    std::optional<InterpreterNodePtr> else_branch = args.size() == 3 ? std::optional{args[2]} : std::nullopt;


    auto condition = args[0];
    auto evaluated = get_or_ret(eval_to_bool(interpreter, condition));

    if (evaluated) {
      return eval(interpreter, true_branch);
    } else if (else_branch.has_value()) {
      return eval(interpreter, *else_branch);
    } else {
      return null_node();
    }
  }
};

class WhileSpecialNode : public SpecialFormNode {
public:
  std::string description() const override { return "while"; }

  EvaluationResult<InterpreterNodePtr> evaluate(
      NotNullSharedPtr<InterpreterNode> self,
      Interpreter& interpreter,
      std::deque<NotNullSharedPtr<InterpreterNode>> args
    ) const override {
    expect_n_args(args, 2);

    auto condition = args[0];
    auto body = args[1];
    while (get_or_ret(eval_to_bool(interpreter, condition))) {
      EvaluationResult<InterpreterNodePtr> result = eval(interpreter, body);
      if (result.is_bre()) {
        break;
      }
      if (!result.is_ok()) {
        return result.to_throw();
      }
    }

    return null_node();
  }
};


class BreakSpecialNode : public SpecialFormNode {
public:
  std::string description() const override { return "break"; }

  EvaluationResult<InterpreterNodePtr> evaluate(
      NotNullSharedPtr<InterpreterNode> self,
      Interpreter& interpreter,
      std::deque<NotNullSharedPtr<InterpreterNode>> args
    ) const override {
    return EvaluationResult<InterpreterNodePtr>::bre();
  }
};


class ReturnSpecialNode : public SpecialFormNode {
public:
  std::string description() const override { return "return"; }

  EvaluationResult<InterpreterNodePtr> evaluate(
      NotNullSharedPtr<InterpreterNode> self,
      Interpreter& interpreter,
      std::deque<NotNullSharedPtr<InterpreterNode>> args
    ) const override {
    expect_n_args(args, 1);
    auto return_value = get_or_ret(eval(interpreter, args[0]));
    return EvaluationResult<InterpreterNodePtr>::ret(return_value);
  }
};



class ProgSpecialNode : public SpecialFormNode {
public:
  std::string description() const override { return "prog"; }

  EvaluationResult<InterpreterNodePtr> evaluate(
      NotNullSharedPtr<InterpreterNode> self,
      Interpreter& interpreter,
      std::deque<NotNullSharedPtr<InterpreterNode>> args
    ) const override {
    expect_n_args(args, 2);

    auto context_vars = force_downcast<List>(interpreter, args[0]);
    std::unordered_set<std::string> exceptions{};
    for (auto i : context_vars->elements) {
      auto exception = force_downcast<Atom>(interpreter, i);
      
      exceptions.emplace(exception->identifier->name);
    }
    auto token = interpreter.get_context().create_layer(exceptions);

    auto body_opt = args[1].dyn_cast<List>();

    interpreter.assert_verbose(args[1], body_opt, "Second argument of prog must be List");
    auto body = *body_opt;

    InterpreterNodePtr last_result = null_node();
    for (auto v : body->elements) {
      last_result = get_or_ret(eval(interpreter, v));
    }
    
    return last_result;
  }
};


template<typename... T>
class SimpleFunction : public SpecialFormNode {
private:

  std::optional<EvaluationResult<InterpreterNodePtr>> first_error;

  template<class M>
    std::optional<EvaluationResult<NotNullSharedPtr<M>>> pop_type(std::deque<InterpreterNodePtr>& args,
        InterpreterNodePtr node,
        Interpreter& interpreter) const {
    if (first_error.has_value()) {
      return std::nullopt;
    }
    auto term = args.front();
    args.pop_front();
    EvaluationResult<NotNullSharedPtr<M>> evaluated = eval_to_type<M>(interpreter, term);

    if (evaluated.get_type() != EvaluationResultType::OK) {
      const_cast<SimpleFunction*>(this)->first_error.emplace(std::move(evaluated));
      return std::nullopt;
    }
    return evaluated;
  }

  EvaluationResult<InterpreterNodePtr> execute(Interpreter& interpreter, const std::optional<EvaluationResult<NotNullSharedPtr<T>>>&... args) const {
    if (first_error.has_value()) {
      return first_error.value().to_throw();
    }

    return run(interpreter, (*args).get_value()...);
  }

public:

  EvaluationResult<InterpreterNodePtr> evaluate(
      NotNullSharedPtr<InterpreterNode> self,
      Interpreter& interpreter,
      std::deque<NotNullSharedPtr<InterpreterNode>> args
    ) const final {

    expect_n_args(args, sizeof...(T));

    std::tuple<std::optional<EvaluationResult<NotNullSharedPtr<T>>>...> argss{
      pop_type<T>(args, self, interpreter)...
    };

    auto invoker = [&]<typename... IT>(const IT&... args) { 
      return execute(interpreter, args...);
    };

    return std::apply(invoker, argss);
  }

  virtual EvaluationResult<InterpreterNodePtr> run(Interpreter& interpreter,
      NotNullSharedPtr<T>...) const = 0;
};

template<typename F>
class BinaryMathFunction : public SimpleFunction<Literal, Literal>
                         , public std::enable_shared_from_this<BinaryMathFunction<F>> {
private:
  F func;
  std::string name;

public:
  BinaryMathFunction(std::string name, F&& func) : name(std::move(name)), func(func) { }

  std::string description() const override { return name; }

  EvaluationResult<InterpreterNodePtr> run(Interpreter& interpreter, NotNullSharedPtr<Literal> a, NotNullSharedPtr<Literal> b) const override {
    interpreter.assert_verbose(a, a->type != Literal::Type::BOOLEAN, "first operand must be digital");
    interpreter.assert_verbose(a, a->type != Literal::Type::NULLVAL, "first operand must be digital");
    interpreter.assert_verbose(b, b->type != Literal::Type::BOOLEAN, "second operand must be digital");
    interpreter.assert_verbose(b, b->type != Literal::Type::NULLVAL, "second operand must be digital");

    Literal::Type target_type = a->type == Literal::Type::REAL || b->type == Literal::Type::REAL 
      ? Literal::Type::REAL 
      : Literal::Type::INTEGER;


    NotNullSharedPtr<Literal> r;
    if (target_type == Literal::Type::INTEGER) {
      r = makeLiteralInt(std::nullopt, func(a->intValue, b->intValue));
    } else if (a->type == Literal::Type::INTEGER) {
      r = makeLiteralReal(std::nullopt, func(a->intValue, b->realValue));
    } else if (b->type == Literal::Type::INTEGER) {
      r = makeLiteralReal(std::nullopt, func(a->realValue, b->intValue));
    } else {
      r = makeLiteralReal(std::nullopt, func(a->realValue, b->realValue));
    }
    return r;
  }
};

template<template <typename> typename T>
auto make_form(std::string name, auto&& f) -> NotNullSharedPtr<T<std::remove_cvref_t<decltype(f)>>> {
  return make_nn_shared<T<std::remove_cvref_t<decltype(f)>>>(std::move(name), f);
}

template<typename F>
NotNullSharedPtr<BinaryMathFunction<F>> make_bin_math_func(std::string name, F&& f) {
  return NotNullSharedPtr<BinaryMathFunction<F>>{new BinaryMathFunction{std::move(name), std::forward<F>(f)}};
}


NotNullSharedPtr<SpecialFormNode> plus_node = make_bin_math_func("plus", [](auto a, auto b){ return a + b; });
NotNullSharedPtr<SpecialFormNode> minus_node = make_bin_math_func("minus", [](auto a, auto b){ return a - b; });
NotNullSharedPtr<SpecialFormNode> times_node = make_bin_math_func("times", [](auto a, auto b){ return a * b; });
NotNullSharedPtr<SpecialFormNode> divide_node = make_bin_math_func("divide", [](auto a, auto b){ return a / b; });

class ModFunction : public SimpleFunction<Literal, Literal> {
public:
  std::string description() const override { return "mod"; }

  EvaluationResult<InterpreterNodePtr> run(Interpreter& interpreter, NotNullSharedPtr<Literal> a, NotNullSharedPtr<Literal> b) const override {
    interpreter.assert_verbose(a, a->type == Literal::Type::INTEGER, "first operand must be int");
    interpreter.assert_verbose(b, b->type == Literal::Type::INTEGER, "second operand must be int");

    return makeLiteralInt(std::nullopt, a->intValue % b->intValue);
  }
};

class HeadFunction : public SimpleFunction<List> {
public:
  std::string description() const override { return "head"; }

  EvaluationResult<InterpreterNodePtr> run(Interpreter& interpreter, NotNullSharedPtr<List> arg) const override {
    EvaluatonException::throw_if_false(arg->elements.size() > 0, "list must not be empty");
    return arg->elements[0];
  }
};

class LengthFunction : public SimpleFunction<List> {
public:
  std::string description() const override { return "length"; }

  EvaluationResult<InterpreterNodePtr> run(Interpreter& interpreter,
      NotNullSharedPtr<List> arg) const override {
    return makeLiteralInt(std::nullopt, arg->elements.size());
  }
};

class TailFunction : public SimpleFunction<List> {
public:
  std::string description() const override { return "tail"; }

  EvaluationResult<InterpreterNodePtr> run(Interpreter& interpreter, NotNullSharedPtr<List> arg) const override {
    interpreter.assert_verbose(arg, arg->elements.size() > 0, "list must not be empty");
    std::vector<InterpreterNodeNNPtr> tail{};
    std::ranges::copy(arg->elements.begin() + 1, arg->elements.end(), std::back_inserter(tail));
    return make_nn_shared<List>(std::nullopt, std::move(tail));
  }
};

class ConsFunction : public SimpleFunction<InterpreterNode, List> {
public:
  std::string description() const override { return "cons"; }

  EvaluationResult<InterpreterNodePtr> run(Interpreter& interpreter,
      InterpreterNodePtr val,
      NotNullSharedPtr<List> arg) const override {
    std::vector<InterpreterNodeNNPtr> result{};
    result.push_back(val);
    std::ranges::copy(arg->elements.begin(),
        arg->elements.end(),
        std::back_inserter(result));
    return make_nn_shared<List>(std::nullopt, result);
  }
};

template<typename F>
class LiteralBiFunction : public SimpleFunction<Literal, Literal> 
                        , public std::enable_shared_from_this<LiteralBiFunction<F>> {
private:
  F func;
  std::string name;
public:
  LiteralBiFunction(std::string name, F func) : func(func), name(name) {}

  std::string description() const override { return name; }

  EvaluationResult<InterpreterNodePtr> run(Interpreter& interpreter, NotNullSharedPtr<Literal> a, NotNullSharedPtr<Literal> b) const override {
    bool value = func(interpreter, *a.get(), *b.get());
    return makeLiteralBool(std::nullopt, value);
  }
};

NotNullSharedPtr<SpecialFormNode> equal = make_form<LiteralBiFunction>("equal", [](const Interpreter& interpreter, const Literal& a, const Literal& b) { return a.eq(interpreter, b); });
NotNullSharedPtr<SpecialFormNode> nonequal = make_form<LiteralBiFunction>("nonequal", [](const Interpreter& interpreter, const Literal& a, const Literal& b) { return a.neq(interpreter, b); });
NotNullSharedPtr<SpecialFormNode> less = make_form<LiteralBiFunction>("less", [](const Interpreter& interpreter, const Literal& a, const Literal& b) { return a.less(interpreter, b); });
NotNullSharedPtr<SpecialFormNode> lesseq = make_form<LiteralBiFunction>("lesseq", [](const Interpreter& interpreter, const Literal& a, const Literal& b) { return a.lesseq(interpreter, b); });
NotNullSharedPtr<SpecialFormNode> greater = make_form<LiteralBiFunction>("greater", [](const Interpreter& interpreter, const Literal& a, const Literal& b) { return a.greater(interpreter, b); });
NotNullSharedPtr<SpecialFormNode> greatereq = make_form<LiteralBiFunction>("greatereq", [](const Interpreter& interpreter, const Literal& a, const Literal& b) { return a.greatereq(interpreter, b); });

template<typename F>
class ElementPredicate : public SimpleFunction<InterpreterNode> 
                        , public std::enable_shared_from_this<ElementPredicate<F>> {
private:
  F func;
  std::string name;
public:
  ElementPredicate(std::string name, F func) : func(func), name(name) {}

  std::string description() const override { return name; }

  EvaluationResult<InterpreterNodePtr> run(Interpreter& interpreter, NotNullSharedPtr<InterpreterNode> elem) const override {
    bool value = func(*elem.get());
    return makeLiteralBool(std::nullopt, value);
  }
};

template<typename F>
std::function<bool(const InterpreterNode&)> make_literal_predicate(F&& f) {
  return [&](const InterpreterNode& elem){
    const Literal* literal = dynamic_cast<const Literal*>(&elem);
    if (literal == nullptr) {
      return false;
    }
    return f(*literal);
  };
}

NotNullSharedPtr<SpecialFormNode> isint = make_form<ElementPredicate>("isint", make_literal_predicate([](const Literal& literal){ return literal.type == Literal::Type::INTEGER; }));
NotNullSharedPtr<SpecialFormNode> isreal = make_form<ElementPredicate>("isreal", make_literal_predicate([](const Literal& literal){ return literal.type == Literal::Type::REAL; }));
NotNullSharedPtr<SpecialFormNode> isbool = make_form<ElementPredicate>("isbool", make_literal_predicate([](const Literal& literal){ return literal.type == Literal::Type::BOOLEAN; }));
NotNullSharedPtr<SpecialFormNode> isnull = make_form<ElementPredicate>("isnull", make_literal_predicate([](const Literal& literal){ return literal.type == Literal::Type::NULLVAL; }));
NotNullSharedPtr<SpecialFormNode> isatom = make_form<ElementPredicate>("isatom", [](const InterpreterNode& elem){ return dynamic_cast<const Atom*>(&elem) != nullptr; });
NotNullSharedPtr<SpecialFormNode> islist = make_form<ElementPredicate>("islist", [](const InterpreterNode& elem){ return dynamic_cast<const List*>(&elem) != nullptr; });

template<typename F>
class BoolBiFunction : public SimpleFunction<Literal, Literal> 
                        , public std::enable_shared_from_this<BoolBiFunction<F>> {
private:
  F func;
  std::string name;
public:
  BoolBiFunction(std::string name, F func) : func(func), name(name) {}

  std::string description() const override { return name; }

  EvaluationResult<InterpreterNodePtr> run(Interpreter& interpreter, NotNullSharedPtr<Literal> a, NotNullSharedPtr<Literal> b) const override {
    interpreter.assert_verbose(a, a->type == Literal::Type::BOOLEAN, "first operand of {} must be bool", name.c_str());
    interpreter.assert_verbose(b, b->type == Literal::Type::BOOLEAN, "second operand of {} must be bool", name.c_str());

    bool value = func(a->boolValue, b->boolValue);
    return makeLiteralBool(std::nullopt, value);
  }
};

NotNullSharedPtr<SpecialFormNode> and_func = make_form<BoolBiFunction>("and", [](bool a, bool b){ return a && b; });
NotNullSharedPtr<SpecialFormNode> or_func = make_form<BoolBiFunction>("or", [](bool a, bool b){ return a || b; });
NotNullSharedPtr<SpecialFormNode> xor_func = make_form<BoolBiFunction>("xor", [](bool a, bool b){ return a != b; });


class BoolNotFunction : public SimpleFunction<Literal> {
public:
  std::string description() const override { return "not"; }

  EvaluationResult<InterpreterNodePtr> run(Interpreter& interpreter, NotNullSharedPtr<Literal> a) const override {
    interpreter.assert_verbose(a, a->type == Literal::Type::BOOLEAN, "operand of {} must be bool", description().c_str());

    bool value = !a->boolValue;
    return makeLiteralBool(std::nullopt, value);
  }
};

class EvalFunction : public SimpleFunction<InterpreterNode> {
public:
  std::string description() const override { return "eval"; }

  EvaluationResult<InterpreterNodePtr> run(Interpreter& interpreter, NotNullSharedPtr<InterpreterNode> a) const override {
    return a;
  }
};

class PrintFunction : public SimpleFunction<InterpreterNode> {
public:
  std::string description() const override { return "print"; }

  EvaluationResult<InterpreterNodePtr> run(Interpreter& interpreter, NotNullSharedPtr<InterpreterNode> a) const override {
    a->print(std::cout);
    std::cout << std::endl;
    return null_node();
  }
};


void register_special_forms(Context& context) {
  std::vector<NotNullSharedPtr<SpecialFormNode>> f{};
  f.emplace_back(make_nn_shared<QuoteSpecialNode>());
  f.emplace_back(make_nn_shared<SetqSpecialNode>());
  f.emplace_back(make_nn_shared<LambdaSpecialNode>());
  f.emplace_back(make_nn_shared<FuncSpecialNode>());
  f.emplace_back(make_nn_shared<CondSpecialNode>());
  f.emplace_back(make_nn_shared<WhileSpecialNode>());
  f.emplace_back(make_nn_shared<BreakSpecialNode>());
  f.emplace_back(make_nn_shared<ReturnSpecialNode>());
  f.emplace_back(make_nn_shared<ProgSpecialNode>());

  f.emplace_back(plus_node);
  f.emplace_back(minus_node);
  f.emplace_back(times_node);
  f.emplace_back(divide_node);
  f.emplace_back(make_nn_shared<ModFunction>());

  f.emplace_back(make_nn_shared<HeadFunction>());
  f.emplace_back(make_nn_shared<TailFunction>());
  f.emplace_back(make_nn_shared<ConsFunction>());
  f.emplace_back(make_nn_shared<LengthFunction>());

  f.emplace_back(equal);
  f.emplace_back(nonequal);
  f.emplace_back(less);
  f.emplace_back(lesseq);
  f.emplace_back(greater);
  f.emplace_back(greatereq);

  f.emplace_back(isint);
  f.emplace_back(isreal);
  f.emplace_back(isbool);
  f.emplace_back(isnull);
  f.emplace_back(isatom);
  f.emplace_back(islist);

  f.emplace_back(and_func);
  f.emplace_back(or_func);
  f.emplace_back(xor_func);
  f.emplace_back(make_nn_shared<BoolNotFunction>());

  f.emplace_back(make_nn_shared<EvalFunction>());

  f.emplace_back(make_nn_shared<PrintFunction>());

  for (auto node : f) {
    context.set_in_root(node->description(), node);
  }
}
