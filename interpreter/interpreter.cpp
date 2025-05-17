#include "interpreter/interpreter.hpp"
#include "utils/utils.hpp"
#include <optional>




std::shared_ptr<InterpreterNode> Context::ContextLayer::get(std::string_view name) {
  auto r = this->variables.find(name);
  if (r != this->variables.end()) {
    return r->second;
  } else {
    return nullptr;
  }
}

void Context::ContextLayer::set(std::string_view name, std::shared_ptr<InterpreterNode> value) {
  this->variables[std::string{name}] = value;
}

std::shared_ptr<InterpreterNode> Context::get(std::string_view name) {
  for (auto layer : this->layers) {
   auto v = layer.get(name);
   if (v) {
     return v;
   }
  }
  return nullptr;
}

void Context::set(std::string_view name, std::shared_ptr<InterpreterNode> value) {
  this->layers[0].set(name, value);
}

void Context::set_in_root(std::string_view name, std::shared_ptr<InterpreterNode> value) {
  this->root_layer.set(name, value);
}

Context::Context() : root_layer((layers.emplace_back(), layers.back())) { }

void Context::pop_layer() {
  assert(layers.size() > 1, "layers must be > 1");
  layers.pop_front();
}

[[nodiscard]]
Context::ContextLayerToken Context::create_layer() {
  layers.emplace_front();
  return Context::ContextLayerToken(*this);
}

void InterpreterStack::push(std::shared_ptr<InterpreterNode> value) {
  this->content.push_back(value);
}
std::shared_ptr<InterpreterNode> InterpreterStack::pop() {
  assert(this->content.size() > 0, "stack is empty");
  auto v = this->content.back();

  this->content.pop_back();

  return v;
}

void InterpreterStack::assert_empty() {
  assert(content.empty(), "assert empty");
}

std::optional<std::shared_ptr<InterpreterNode>> InterpreterStack::pop_or_null() {
  if (this->content.size() < 1) {
    return pop();
  }
  return std::nullopt;
}
