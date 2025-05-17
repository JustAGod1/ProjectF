#include "interpreter/special_forms.hpp"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <string_view>
#include <sstream>
#include <type_traits>
#include <utility>
#include "interpreter/interpreter.hpp"
#include "parser/parser_structs.hpp"
#include "utils/utils.hpp"

static std::vector<std::shared_ptr<InterpreterNode>> get_args(Interpreter& interpreter, int count) {
  std::vector<std::shared_ptr<InterpreterNode>> result;
  for (int i = 0; i < count; i++) {
    result.emplace_back(interpreter.get_stack().pop());
  }
  
  interpreter.get_stack().assert_empty();
  return result;
}

class SpecialFormNode : public CallableNode {
public:
  virtual std::string description() const = 0;
  void print(std::ostream& out, int indent = 0) const {
    out << "Special Form " << description();
  }
};

class QuoteSpecialNode : public SpecialFormNode {
public:
  std::string description() const override { return "quote"; }

  EvaluationResult<InterpreterNodePtr> evaluate(
      std::shared_ptr<InterpreterNode> self,
      Interpreter& interpreter
    ) const override {
    return get_args(interpreter, 1)[0];
  }
};

static EvaluationResult<std::shared_ptr<InterpreterNode>> eval(Interpreter& interpreter, std::shared_ptr<InterpreterNode> node) {
  return node->evaluate(node, interpreter);
}

template <class Derive>
static EvaluationResult<std::shared_ptr<Derive>> eval_to_type(Interpreter& interpreter, std::shared_ptr<InterpreterNode> node) {
  auto raw = get_or_ret(eval(interpreter, node));
  auto downcasted = std::dynamic_pointer_cast<Derive>(raw);
  if (!downcasted) {
    std::ostringstream sstream{};
    node->print(sstream);
    assert(downcasted, "%s expected to be evaluated to %s but evaluated to %s", sstream.str().c_str(), typeid(Derive).name(), typeid(raw.get()).name());
  }
  return downcasted;
}

static EvaluationResult<bool> eval_to_bool(Interpreter& interpreter, std::shared_ptr<InterpreterNode> node) {
  auto literal = get_or_ret(eval_to_type<Literal>(interpreter, node));
  if (literal->type != Literal::Type::BOOLEAN) {
    std::ostringstream sstream{};
    node->print(sstream);
    assert(literal->type != Literal::Type::BOOLEAN,
        "%s expected to be evaluated to BOOLEAN but evaluated to %s",
        sstream.str().c_str(),
        literal->type);
  }

  return literal->boolValue;
}

template <class Derive>
std::shared_ptr<Derive> force_downcast(InterpreterNodePtr base) {
  auto downcasted = std::dynamic_pointer_cast<Derive>(base);
  assert(downcasted, "Failed to cast to %s", typeid(Derive).name());
  return downcasted;
}

class SetqSpecialNode : public SpecialFormNode {
public:
  std::string description() const override { return "setq"; }

  EvaluationResult<InterpreterNodePtr> evaluate(
      std::shared_ptr<InterpreterNode> self,
      Interpreter& interpreter
    ) const override {
    std::vector<std::shared_ptr<InterpreterNode>> args = get_args(interpreter, 2);

    auto maybe_atom = force_downcast<Atom>(args[0]);
    
    auto value = get_or_ret(eval(interpreter, args[1]));

    interpreter.get_context().set(maybe_atom->identifier->name, value);

    return NULL_NODE;
  }
};

class LambdaValueNode : public CallableNode {
private:
  std::vector<std::string> arg_names;
  InterpreterNodePtr body;
public:
  explicit LambdaValueNode(std::vector<std::string> arg_names, InterpreterNodePtr body) : arg_names(arg_names), body(body) {}


  EvaluationResult<InterpreterNodePtr> evaluate(
      std::shared_ptr<InterpreterNode> self,
      Interpreter& interpreter
    ) const override {
    auto tok = interpreter.get_context().create_layer();
    auto args = get_args(interpreter, arg_names.size());
    for (int i = 0; i < args.size(); i++) {
      interpreter.get_context().set(arg_names[i], args[i]);
    }
    
    auto r = body->evaluate(body, interpreter);
    
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
      std::shared_ptr<InterpreterNode> self,
      Interpreter& interpreter
    ) const override {
    std::vector<std::shared_ptr<InterpreterNode>> args = get_args(interpreter, 2);

    auto maybe_list = force_downcast<List>(args[0]);

    std::vector<std::string> names;
    std::ranges::transform(maybe_list->elements, std::back_inserter(names), [&](auto &v) {
        std::shared_ptr<Atom> vd = force_downcast<Atom>(v);
        return vd->identifier->name;
      });

    auto body = args[1];
    
    return std::make_shared<LambdaValueNode>(names, body);
  }
};

class FuncSpecialNode : public SpecialFormNode {
public:
  std::string description() const override { return "lambda"; }

  EvaluationResult<InterpreterNodePtr> evaluate(
      std::shared_ptr<InterpreterNode> self,
      Interpreter& interpreter
    ) const override {
    std::vector<std::shared_ptr<InterpreterNode>> args = get_args(interpreter, 3);

    auto maybe_atom = force_downcast<Atom>(args[0]);

    auto maybe_list = force_downcast<List>(args[1]);

    std::vector<std::string> names;
    std::ranges::transform(maybe_list->elements, std::back_inserter(names), [&](auto &v) {
        std::shared_ptr<Atom> vd = force_downcast<Atom>(v);
        return vd->identifier->name;
      });
    
    auto body = args[2];

    auto v = std::make_shared<LambdaValueNode>(names, body);

    interpreter.get_context().set(maybe_atom->identifier->name, v);

    return NULL_NODE;
  }
};

class CondSpecialNode : public SpecialFormNode {
public:
  std::string description() const override { return "cond"; }

  EvaluationResult<InterpreterNodePtr> evaluate(
      std::shared_ptr<InterpreterNode> self,
      Interpreter& interpreter
    ) const override {
    std::vector<InterpreterNodePtr> args = get_args(interpreter, 2);

    InterpreterNodePtr true_branch = args[1];
    std::optional<InterpreterNodePtr> else_branch = interpreter.get_stack().pop_or_null();

    interpreter.get_stack().assert_empty();

    auto condition = args[0];
    auto evaluated = get_or_ret(eval_to_bool(interpreter, condition));

    if (evaluated) {
      return eval(interpreter, true_branch);
    } else if (else_branch.has_value()) {
      return eval(interpreter, *else_branch);
    } else {
      return NULL_NODE;
    }
  }
};

class WhileSpecialNode : public SpecialFormNode {
public:
  std::string description() const override { return "while"; }

  EvaluationResult<InterpreterNodePtr> evaluate(
      std::shared_ptr<InterpreterNode> self,
      Interpreter& interpreter
    ) const override {
    std::vector<InterpreterNodePtr> args = get_args(interpreter, 2);

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

    return NULL_NODE;
  }
};


class BreakSpecialNode : public SpecialFormNode {
public:
  std::string description() const override { return "break"; }

  EvaluationResult<InterpreterNodePtr> evaluate(
      std::shared_ptr<InterpreterNode> self,
      Interpreter& interpreter
    ) const override {
    return EvaluationResult<InterpreterNodePtr>::bre();
  }
};


class ReturnSpecialNode : public SpecialFormNode {
public:
  std::string description() const override { return "return"; }

  EvaluationResult<InterpreterNodePtr> evaluate(
      std::shared_ptr<InterpreterNode> self,
      Interpreter& interpreter
    ) const override {
    std::vector<InterpreterNodePtr> args = get_args(interpreter, 1);
    auto return_value = get_or_ret(eval(interpreter, args[0]));
    return EvaluationResult<InterpreterNodePtr>::ret(return_value);
  }
};



class ProgSpecialNode : public SpecialFormNode {
public:
  std::string description() const override { return "prog"; }

  EvaluationResult<InterpreterNodePtr> evaluate(
      std::shared_ptr<InterpreterNode> self,
      Interpreter& interpreter
    ) const override {
    std::vector<InterpreterNodePtr> args = get_args(interpreter, 2);

    auto token = interpreter.get_context().create_layer();
    auto context_vars = force_downcast<List>(args[0]);
    for (auto i : context_vars->elements) {
      auto entry = force_downcast<List>(i);
      
      assert(entry->elements.size() != 2, "every elemnt of first argument of prog must contain two elements");

      auto atom = force_downcast<Atom>(entry->elements[0]);
      auto value = get_or_ret(eval(interpreter, entry->elements[0]));


      interpreter.get_context().set(atom->identifier->name, value);
    }

    auto body = args[1];

    if (dynamic_cast<List*>(body.get())) {
      auto list = std::static_pointer_cast<List>(body);

      InterpreterNodePtr last_value;

      for (auto v : list->elements) {
        last_value = get_or_ret(eval(interpreter, v));
      }

      return last_value;
    } else {
      return eval(interpreter, body);
    }
  }
};


template<typename... T>
class SimpleFunction : public SpecialFormNode {
private:

  std::optional<EvaluationResult<InterpreterNodePtr>> first_error;

  template<class M>
  EvaluationResult<std::shared_ptr<M>> pop_type(Interpreter& interpreter) const {
    if (first_error.has_value()) {
      return nullptr;
    }
    auto term = interpreter.get_stack().pop();
    auto evaluated = eval(interpreter, term);
    if (evaluated.get_type() != EvaluationResultType::OK) {
      const_cast<SimpleFunction*>(this)->first_error.emplace(std::move(evaluated));
      return nullptr;
    }
    return force_downcast<M>(evaluated.get_value());
  }

  EvaluationResult<InterpreterNodePtr> execute(EvaluationResult<std::shared_ptr<T>>... args) const {
    if (first_error.has_value()) {
      return first_error.value().to_throw();
    }

    return run(args.get_value()...);
  }
public:

  EvaluationResult<InterpreterNodePtr> evaluate(
      std::shared_ptr<InterpreterNode> self,
      Interpreter& interpreter
    ) const final {
    return execute(pop_type<T>(interpreter)...);
  }

  virtual EvaluationResult<InterpreterNodePtr> run(std::shared_ptr<T>...) const = 0;
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

  EvaluationResult<InterpreterNodePtr> run(std::shared_ptr<Literal> a, std::shared_ptr<Literal> b) const override {
    assert(a->type != Literal::Type::BOOLEAN, "first operand must be digital");
    assert(a->type != Literal::Type::NULLVAL, "first operand must be digital");
    assert(b->type != Literal::Type::BOOLEAN, "second operand must be digital");
    assert(b->type != Literal::Type::NULLVAL, "second operand must be digital");

    Literal::Type target_type = a->type == Literal::Type::REAL || b->type == Literal::Type::REAL 
      ? Literal::Type::REAL 
      : Literal::Type::INTEGER;


    if (target_type == Literal::Type::INTEGER) {
      return makeLiteralInt(func(a->intValue, b->intValue));
    } else if (a->type == Literal::Type::INTEGER) {
      return makeLiteralReal(func(a->intValue, b->realValue));
    } else if (b->type == Literal::Type::INTEGER) {
      return makeLiteralReal(func(a->realValue, b->intValue));
    } else {
      return makeLiteralReal(func(a->realValue, b->realValue));
    }
  }
};

template<template <typename> typename T>
auto make_form(std::string name, auto&& f) -> std::shared_ptr<T<std::remove_cvref_t<decltype(f)>>> {
  return std::make_shared<T<std::remove_cvref_t<decltype(f)>>>(std::move(name), f);
}

template<typename F>
std::shared_ptr<BinaryMathFunction<F>> make_bin_math_func(std::string name, F&& f) {
  return std::shared_ptr<BinaryMathFunction<F>>{new BinaryMathFunction{std::move(name), std::forward<F>(f)}};
}


std::shared_ptr<SpecialFormNode> plus_node = make_bin_math_func("plus", [](auto a, auto b){ return a + b; });
std::shared_ptr<SpecialFormNode> minus_node = make_bin_math_func("minus", [](auto a, auto b){ return a - b; });
std::shared_ptr<SpecialFormNode> times_node = make_bin_math_func("times", [](auto a, auto b){ return a * b; });
std::shared_ptr<SpecialFormNode> divide_node = make_bin_math_func("divide", [](auto a, auto b){ return a / b; });

class HeadFunction : public SimpleFunction<List> {
public:
  std::string description() const override { return "head"; }

  EvaluationResult<InterpreterNodePtr> run(std::shared_ptr<List> arg) const override {
    assert(arg->elements.size() > 0, "list must not be empty");
    return arg->elements[0];
  }
};

class TailFunction : public SimpleFunction<List> {
public:
  std::string description() const override { return "tail"; }

  EvaluationResult<InterpreterNodePtr> run(std::shared_ptr<List> arg) const override {
    assert(arg->elements.size() > 0, "list must not be empty");
    std::vector<InterpreterNodePtr> tail{};
    std::ranges::copy(arg->elements.begin() + 1, arg->elements.end(), std::back_inserter(tail));
    return std::make_shared<List>(std::move(tail));
  }
};

class ConsFunction : public SimpleFunction<InterpreterNode, List> {
public:
  std::string description() const override { return "cons"; }

  EvaluationResult<InterpreterNodePtr> run(InterpreterNodePtr val, std::shared_ptr<List> arg) const override {
    assert(arg->elements.size() > 0, "list must not be empty");
    std::vector<InterpreterNodePtr> result{};
    result.push_back(val);
    std::ranges::copy(arg->elements.begin(), arg->elements.end(), std::back_inserter(result));
    return std::make_shared<List>(result);
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

  EvaluationResult<InterpreterNodePtr> run(std::shared_ptr<Literal> a, std::shared_ptr<Literal> b) const override {
    bool value = func(*a.get(), *b.get());
    return makeLiteralBool(value);
  }
};

std::shared_ptr<SpecialFormNode> equal = make_form<LiteralBiFunction>("equal", [](const Literal& a, const Literal& b) { return a.eq(b); });
std::shared_ptr<SpecialFormNode> nonequal = make_form<LiteralBiFunction>("nonequal", [](const Literal& a, const Literal& b) { return a.neq(b); });
std::shared_ptr<SpecialFormNode> less = make_form<LiteralBiFunction>("less", [](const Literal& a, const Literal& b) { return a.less(b); });
std::shared_ptr<SpecialFormNode> lesseq = make_form<LiteralBiFunction>("lesseq", [](const Literal& a, const Literal& b) { return a.lesseq(b); });
std::shared_ptr<SpecialFormNode> greater = make_form<LiteralBiFunction>("greater", [](const Literal& a, const Literal& b) { return a.greater(b); });
std::shared_ptr<SpecialFormNode> greatereq = make_form<LiteralBiFunction>("greatereq", [](const Literal& a, const Literal& b) { return a.greatereq(b); });

template<typename F>
class ElementPredicate : public SimpleFunction<InterpreterNode> 
                        , public std::enable_shared_from_this<ElementPredicate<F>> {
private:
  F func;
  std::string name;
public:
  ElementPredicate(std::string name, F func) : func(func), name(name) {}

  std::string description() const override { return name; }

  EvaluationResult<InterpreterNodePtr> run(std::shared_ptr<InterpreterNode> elem) const override {
    bool value = func(*elem.get());
    return makeLiteralBool(value);
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

std::shared_ptr<SpecialFormNode> isint = make_form<ElementPredicate>("isint", make_literal_predicate([](const Literal& literal){ return literal.type == Literal::Type::INTEGER; }));
std::shared_ptr<SpecialFormNode> isreal = make_form<ElementPredicate>("isreal", make_literal_predicate([](const Literal& literal){ return literal.type == Literal::Type::REAL; }));
std::shared_ptr<SpecialFormNode> isbool = make_form<ElementPredicate>("isbool", make_literal_predicate([](const Literal& literal){ return literal.type == Literal::Type::BOOLEAN; }));
std::shared_ptr<SpecialFormNode> isnull = make_form<ElementPredicate>("isnull", make_literal_predicate([](const Literal& literal){ return literal.type == Literal::Type::NULLVAL; }));
std::shared_ptr<SpecialFormNode> isatom = make_form<ElementPredicate>("isatom", [](const InterpreterNode& elem){ return dynamic_cast<const Atom*>(&elem) != nullptr; });
std::shared_ptr<SpecialFormNode> islist = make_form<ElementPredicate>("islist", [](const InterpreterNode& elem){ return dynamic_cast<const List*>(&elem) != nullptr; });

template<typename F>
class BoolBiFunction : public SimpleFunction<Literal, Literal> 
                        , public std::enable_shared_from_this<BoolBiFunction<F>> {
private:
  F func;
  std::string name;
public:
  BoolBiFunction(std::string name, F func) : func(func), name(name) {}

  std::string description() const override { return name; }

  EvaluationResult<InterpreterNodePtr> run(std::shared_ptr<Literal> a, std::shared_ptr<Literal> b) const override {
    assert(a->type == Literal::Type::BOOLEAN, "first operand of %s must be bool", name.c_str());
    assert(b->type == Literal::Type::BOOLEAN, "second operand of %s must be bool", name.c_str());

    bool value = func(a->boolValue, b->boolValue);
    return makeLiteralBool(value);
  }
};

std::shared_ptr<SpecialFormNode> and_func = make_form<BoolBiFunction>("and", [](bool a, bool b){ return a && b; });
std::shared_ptr<SpecialFormNode> or_func = make_form<BoolBiFunction>("or", [](bool a, bool b){ return a || b; });
std::shared_ptr<SpecialFormNode> xor_func = make_form<BoolBiFunction>("or", [](bool a, bool b){ return a != b; });


class BoolNotFunction : public SimpleFunction<Literal> {
public:
  std::string description() const override { return "not"; }

  EvaluationResult<InterpreterNodePtr> run(std::shared_ptr<Literal> a) const override {
    assert(a->type == Literal::Type::BOOLEAN, "operand of %s must be bool", description().c_str());

    bool value = !a->boolValue;
    return makeLiteralBool(value);
  }
};

class EvalFunction : public SimpleFunction<InterpreterNode> {
public:
  std::string description() const override { return "eval"; }

  EvaluationResult<InterpreterNodePtr> run(std::shared_ptr<InterpreterNode> a) const override {
    return a;
  }
};

class PrintFunction : public SimpleFunction<InterpreterNode> {
public:
  std::string description() const override { return "print"; }

  EvaluationResult<InterpreterNodePtr> run(std::shared_ptr<InterpreterNode> a) const override {
    a->print(std::cout);
    std::cout << std::endl;
    return NULL_NODE;
  }
};


void register_special_forms(Context& context) {
  std::vector<std::shared_ptr<SpecialFormNode>> f{};
  f.emplace_back(std::make_shared<QuoteSpecialNode>());
  f.emplace_back(std::make_shared<SetqSpecialNode>());
  f.emplace_back(std::make_shared<LambdaSpecialNode>());
  f.emplace_back(std::make_shared<FuncSpecialNode>());
  f.emplace_back(std::make_shared<CondSpecialNode>());
  f.emplace_back(std::make_shared<WhileSpecialNode>());
  f.emplace_back(std::make_shared<BreakSpecialNode>());
  f.emplace_back(std::make_shared<ReturnSpecialNode>());
  f.emplace_back(std::make_shared<ProgSpecialNode>());
  f.emplace_back(plus_node);
  f.emplace_back(minus_node);
  f.emplace_back(times_node);
  f.emplace_back(divide_node);
  f.emplace_back(std::make_shared<HeadFunction>());
  f.emplace_back(std::make_shared<TailFunction>());
  f.emplace_back(std::make_shared<ConsFunction>());

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
  f.emplace_back(std::make_shared<BoolNotFunction>());

  f.emplace_back(std::make_shared<EvalFunction>());

  f.emplace_back(std::make_shared<PrintFunction>());

  for (auto node : f) {
    context.set_in_root(node->description(), node);
  }
}
