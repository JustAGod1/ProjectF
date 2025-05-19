#include "parser/parser_structs.hpp"

#include <algorithm>
#include <deque>
#include <iterator>
#include <memory>
#include <span>
#include <type_traits>
#include "utils/utils.hpp"

std::shared_ptr<InterpreterNode> null_node() {
  return makeLiteralNil(std::nullopt);
}

EvaluationResult<InterpreterNodePtr> Identifier::evaluate(std::shared_ptr<InterpreterNode> self,
    Interpreter& interpreter,
      std::deque<std::shared_ptr<InterpreterNode>> args) const {
  return self;
}

template <class T>
requires std::is_base_of_v<InterpreterNode, T>
static EvaluationResult<std::vector<InterpreterNodePtr>> eval(Interpreter& interpreter, std::span<const std::shared_ptr<T>> nodes) {
  std::vector<std::shared_ptr<InterpreterNode>> mapped;
  for (auto v : nodes) {
    mapped.emplace_back(get_or_ret(v->evaluate(v, interpreter, std::deque<InterpreterNodePtr>{})));
  }

  return mapped;
}

template <class T>
requires std::is_base_of_v<InterpreterNode, T>
static EvaluationResult<std::vector<InterpreterNodePtr>> eval(Interpreter& interpreter, const std::vector<std::shared_ptr<T>>& nodes) {
  return eval<T>(interpreter, std::span{nodes});
}

EvaluationResult<InterpreterNodePtr> Program::evaluate(std::shared_ptr<InterpreterNode> self,
    Interpreter& interpreter,
      std::deque<std::shared_ptr<InterpreterNode>> args) const {
  std::vector<std::shared_ptr<InterpreterNode>> mapped = get_or_ret(eval<Element>(interpreter, this->elements));
  
  if (mapped.size() > 0) {
    return mapped.back();
  }
  return null_node();
}

EvaluationResult<InterpreterNodePtr> List::evaluate(std::shared_ptr<InterpreterNode> self,
    Interpreter& interpreter,
      std::deque<std::shared_ptr<InterpreterNode>> args) const {
  auto& elements = this->elements;

  Atom* f_name;
  if (elements.size() > 0) {
    f_name = dynamic_cast<Atom*>(elements[0].get());
    interpreter.assert_verbose(elements[0], f_name != nullptr, "first element of called list must be Atom");
    std::string_view name = f_name->identifier->name;

    auto node = interpreter.get_context().get(name);
    interpreter.assert_verbose(f_name, node, "there is no function with name {}", name);
    interpreter.assert_verbose(f_name, dynamic_cast<CallableNode*>(node.get()) != nullptr, "attempt to call not-callable");

    std::deque<InterpreterNodePtr> args_to_pass;
    for (auto cur = elements.begin() + 1; cur != elements.end(); cur++) {
      args_to_pass.emplace_back(*cur);
    }
      
    try {
      return node->evaluate(node, interpreter, args_to_pass);
    } catch (EvaluatonException& e) {
      e.exit_fatal(interpreter, self);
    } 
  } else {
    return self;
  }
}

EvaluationResult<InterpreterNodePtr> Quote::evaluate(
  std::shared_ptr<InterpreterNode> self,
  Interpreter& interpreter,
      std::deque<std::shared_ptr<InterpreterNode>> args
) const {
  return this->inner;
}

EvaluationResult<InterpreterNodePtr> Atom::evaluate(
  std::shared_ptr<InterpreterNode> self,
  Interpreter& interpreter,
      std::deque<std::shared_ptr<InterpreterNode>> args
) const {
  std::shared_ptr<InterpreterNode> value = interpreter.get_context().get(this->identifier->name);
  interpreter.assert_verbose(self, value, "there is no variable with name {}", this->identifier->name.c_str());
  return value;
}

EvaluationResult<InterpreterNodePtr> Literal::evaluate(
  std::shared_ptr<InterpreterNode> self,
  Interpreter& Interpreter,
      std::deque<std::shared_ptr<InterpreterNode>> args
) const {
  return self;
}

static int boolToInt(bool v) { return v ? 1 : 0; }


bool Literal::less(const Interpreter& interpreter, const Literal& literal) const {
  interpreter.assert_verbose(this, this->type != Literal::Type::NULLVAL, "Cannot compare NULL");
  interpreter.assert_verbose(&literal, literal.type != Literal::Type::NULLVAL, "Cannot compare NULL");
  switch (this->type) {
    case Literal::Type::BOOLEAN: {
                                   switch (literal.type) {
                                     case Literal::Type::BOOLEAN:
                                       return boolToInt(this->boolValue) < boolToInt(literal.boolValue);
                                     case Literal::Type::REAL:
                                       return boolToInt(this->boolValue) < literal.realValue;
                                     case Literal::Type::INTEGER:
                                       return boolToInt(this->boolValue) < literal.intValue;
                                   }
                                 }
    case Literal::Type::INTEGER: {
                                   switch (literal.type) {
                                     case Literal::Type::BOOLEAN:
                                       return this->intValue < boolToInt(literal.boolValue);
                                     case Literal::Type::REAL:
                                       return this->intValue < literal.realValue;
                                     case Literal::Type::INTEGER:
                                       return this->intValue < literal.intValue;
                                   }
                                 }
    case Literal::Type::REAL: {
                                   switch (literal.type) {
                                     case Literal::Type::BOOLEAN:
                                       return this->realValue < boolToInt(literal.boolValue);
                                     case Literal::Type::REAL:
                                       return this->realValue < literal.realValue;
                                     case Literal::Type::INTEGER:
                                       return this->realValue < literal.intValue;
                                   }
                                 }
  }

  assert_unverbose(false, "impossible");
}

