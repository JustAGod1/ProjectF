#include "interpreter/interpreter.hpp"
#include "fmt/base.h"
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
  for (int i = 1; i < this->layers.size(); i++) {
    auto& layer = this->layers.rbegin()[i];
    auto v = layer.variables.find(name);
    if (v != layer.variables.end()) {
      v->second = value;
      return;
    }
  }
  
  this->layers[0].set(name, value);
}

void Context::set_in_root(std::string_view name, std::shared_ptr<InterpreterNode> value) {
  this->root_layer.set(name, value);
}

Context::Context() : root_layer((layers.emplace_back(), layers.back())) { }

void Context::pop_layer(std::optional<std::unordered_set<std::string>> exceptions) {
  assert_unverbose(layers.size() > 1, "layers must be > 1");
  std::vector<std::pair<std::string, InterpreterNodePtr>> to_retain;
  if (exceptions.has_value()) {
    for (auto [k, v] : layers[0].variables) {
      if (!(*exceptions).contains(k)) {
        to_retain.emplace_back(k, v);
      }
    }
  } 
  layers.pop_front();
  for (auto [k, v] : to_retain) {
    set(k, v);
  }
}

[[nodiscard]]
Context::ContextLayerToken Context::create_layer() {
  layers.emplace_front();
  return Context::ContextLayerToken(*this);
}

Context::ContextLayerToken Context::create_layer(std::unordered_set<std::string> exceptions) {
  layers.emplace_front();
  return Context::ContextLayerToken(*this, std::move(exceptions));
}

void InterpreterStack::push(std::shared_ptr<InterpreterNode> value) {
  this->content.push_back(value);
}
std::shared_ptr<InterpreterNode> InterpreterStack::pop(Interpreter& interpreter, InterpreterNodePtr node) {
  interpreter.assert_verbose(node, this->content.size() > 0, "insufficent args");
  auto v = this->content.back();

  this->content.pop_back();

  return v;
}

bool InterpreterStack::is_empty() {
  return content.empty();
}

std::optional<std::shared_ptr<InterpreterNode>> InterpreterStack::pop_or_null() {
  if (!this->content.empty()) {
    auto v = this->content.back();
    this->content.pop_back();
    return v;
  }
  return std::nullopt;
}
