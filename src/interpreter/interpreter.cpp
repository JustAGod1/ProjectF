#include "interpreter/interpreter.hpp"
#include "fmt/base.h"
#include "utils/utils.hpp"
#include <optional>
#include <sstream>




std::optional<NotNullSharedPtr<InterpreterNode>> Context::ContextLayer::get(std::string_view name) {
  auto r = this->variables.find(name);
  if (r != this->variables.end()) {
    return r->second;
  } else {
    return std::nullopt;
  }
}

void Context::ContextLayer::set(std::string_view name, NotNullSharedPtr<InterpreterNode> value) {
  this->variables.insert_or_assign(std::string{name}, value);
}

std::optional<NotNullSharedPtr<InterpreterNode>> Context::get(std::string_view name) {
  for (auto layer : this->layers) {
   auto v = layer.get(name);
   if (v) {
     return v;
   }
  }
  return std::nullopt;
}

void Context::set(std::string_view name, NotNullSharedPtr<InterpreterNode> value) {
  //fmt::println("{} := {}", name, value->to_string());
  this->layers[0].set(name, value);
}

void Context::set_in_root(std::string_view name, NotNullSharedPtr<InterpreterNode> value) {
  this->root_layer.set(name, value);
}

Context::Context() : root_layer((layers.emplace_back(), layers.back())) { }

void Context::print() const { std::cout << to_string(); }

std::string Context::to_string() const {
  std::stringstream result;

  int num = 0;
  for (auto i = layers.rbegin(); i != layers.rend(); i++) {
    result << "Layer " << num++ << std::endl;

    for (auto [k, v] : i->variables) {
      result << "    " << k << ": " << v->to_string() << std::endl;     
    }

  }

  return result.str();
}

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

void InterpreterStack::push(NotNullSharedPtr<InterpreterNode> value) {
  this->content.push_back(value);
}
NotNullSharedPtr<InterpreterNode> InterpreterStack::pop(Interpreter& interpreter, InterpreterNodePtr node) {
  interpreter.assert_verbose(node, this->content.size() > 0, "insufficent args");
  auto v = this->content.back();

  this->content.pop_back();

  return v;
}

bool InterpreterStack::is_empty() {
  return content.empty();
}

std::optional<NotNullSharedPtr<InterpreterNode>> InterpreterStack::pop_or_null() {
  if (!this->content.empty()) {
    auto v = this->content.back();
    this->content.pop_back();
    return v;
  }
  return std::nullopt;
}
