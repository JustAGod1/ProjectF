#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include "interpreter/interpreter.hpp"
#include "parser/node_location.hpp"
// Forward declarations
class ASTNode;
class Program;
class Element;
class List;
class Atom;
class Literal;
class Identifier;

using ASTNodePtr = NotNullSharedPtr<ASTNode>;


// Base AST Node class
class ASTNode : public InterpreterNode {
public:

  ASTNode(std::optional<NodeLocation> location) : InterpreterNode(location) {}
  using InterpreterNode::InterpreterNode;

  virtual ~ASTNode() = default;
};

// Element can be Atom, Literal, or List
class Element : public ASTNode {
public:
  Element(std::optional<NodeLocation> location) : ASTNode(location) {}
  using ASTNode::ASTNode;

  virtual ~Element() = default;
};

// Identifier: sequence of letters and digits
class Identifier : public ASTNode {
public:
    std::string name;
    using ASTNode::ASTNode;

    Identifier(std::optional<NodeLocation> location, std::string name) : ASTNode(location), name(std::move(name)) {}

    void print(std::ostream& out, int indent = 0) const override {
        out << std::string(indent, ' ') << "Identifier: " << name << "\n";
    }
    EvaluationResult<InterpreterNodePtr> evaluate(
        InterpreterNodePtr self,
        Interpreter& Interpreter,
      std::deque<NotNullSharedPtr<InterpreterNode>> args
      ) const override;
};



// Program: collection of Elements
class Program : public ASTNode {
public:
    std::vector<NotNullSharedPtr<Element>> elements;

    using ASTNode::ASTNode;
    Program(std::optional<NodeLocation> location, std::vector<NotNullSharedPtr<Element>> elements) 
      : ASTNode(location)
      , elements(std::move(elements)) {}
    
    void print(std::ostream& out, int indent = 0) const override {
        out << std::string(indent, ' ') << "Program:\n";
        for (const auto& el : elements) {
            el->print(out, indent + 2);
        }
    }
    EvaluationResult<InterpreterNodePtr> evaluate(
        InterpreterNodePtr self,
        Interpreter& Interpreter,
      std::deque<NotNullSharedPtr<InterpreterNode>> args
      ) const override;
};

// List: ( elements... )
class List : public Element {
public:
    std::vector<NotNullSharedPtr<InterpreterNode>> elements;

    using Element::Element;
    List(std::optional<NodeLocation> location, std::vector<NotNullSharedPtr<InterpreterNode>> elements) : Element(location), elements(std::move(elements)) {}

    void print(std::ostream& out, int indent = 0) const override {
      out << "(";
        for (const auto& el : elements) {
            el->print(out, 0);
            out << " ";
        }
      out << ")";
    }
    EvaluationResult<InterpreterNodePtr> evaluate(
        InterpreterNodePtr self,
        Interpreter& Interpreter,
      std::deque<NotNullSharedPtr<InterpreterNode>> args
      ) const override;
};

class Quote : public Element {
private:
    std::optional<InterpreterNodePtr> inner;
public:

    Quote() : Element(location), inner(std::nullopt) {}
    Quote(std::optional<NodeLocation> location, InterpreterNodePtr inner) : Element(location), inner(inner) {}

    InterpreterNodePtr get_inner() const { return *inner; }

    void print(std::ostream& out, int indent = 0) const override {
        out << std::string(indent, ' ') << "'";
        get_inner()->print(out, indent);
    }


    EvaluationResult<InterpreterNodePtr> evaluate(
        InterpreterNodePtr self,
        Interpreter& Interpreter,
      std::deque<NotNullSharedPtr<InterpreterNode>> args
      ) const override;
};

// Atom: Identifier
class Atom : public Element {
public:
    NotNullSharedPtr<Identifier> identifier;
    using Element::Element;

    Atom(std::optional<NodeLocation> location, NotNullSharedPtr<Identifier> identifier) : Element(location), identifier(identifier) {}
    
    void print(std::ostream& out, int indent = 0) const override {
        out << std::string(indent, ' ') << "Atom: ";
        identifier->print(out, 0);
    }


    EvaluationResult<InterpreterNodePtr> evaluate(
        InterpreterNodePtr self,
        Interpreter& Interpreter,
      std::deque<NotNullSharedPtr<InterpreterNode>> args
      ) const override;
};

// Literal values
class Literal : public Element {
public:
    using Element::Element;
    enum class Type { INTEGER, REAL, BOOLEAN, NULLVAL };
    Type type;
    
    std::string_view type_name() const {
      switch (type) {
        case Type::INTEGER:
          return "INTEGER";
        case Type::REAL:
          return "REAL";
        case Type::BOOLEAN:
          return "BOOLEAN";
        case Type::NULLVAL:
          return "NULLVAL";
      }
      assert_unverbose(false, "WTF");
    }

    union {
        int intValue;
        double realValue;
        bool boolValue;
    };

    Literal(std::optional<NodeLocation> location) : Element(location) {}
    
    void print(std::ostream& out, int indent = 0) const override {
        out << std::string(indent, ' ') << "Literal: ";
        switch (type) {
            case Type::INTEGER: out << intValue << "i"; break;
            case Type::REAL: out << realValue << "d"; break;
            case Type::BOOLEAN: out << (boolValue ? "true" : "false"); break;
            case Type::NULLVAL: out << "null"; break;
        }
    }

    EvaluationResult<InterpreterNodePtr> evaluate(
        InterpreterNodePtr self,
        Interpreter& Interpreter,
        std::deque<NotNullSharedPtr<InterpreterNode>> args
      ) const override;

    bool less(const Interpreter& interpreter, const Literal& literal) const;
    bool eq(const Interpreter& interpreter, const Literal& literal) const { return !less(interpreter, literal) && !literal.less(interpreter, *this); }
    bool neq(const Interpreter& interpreter, const Literal& literal) const { return !eq(interpreter, literal); }
    bool lesseq(const Interpreter& interpreter, const Literal& literal) const { return less(interpreter, literal) || eq(interpreter, literal); }
    bool greater(const Interpreter& interpreter, const Literal& literal) const { return !less(interpreter, literal) && !eq(interpreter, literal);}
    bool greatereq(const Interpreter& interpreter, const Literal& literal) const { return greater(interpreter, literal) || eq(interpreter, literal); }
};

inline NotNullSharedPtr<Literal> makeLiteralInt(std::optional<NodeLocation> location, int intValue) {
  auto l = make_nn_shared<Literal>(location);
  l->intValue = intValue;
  l->type = Literal::Type::INTEGER;

  return l;
}

inline NotNullSharedPtr<Literal> makeLiteralReal(std::optional<NodeLocation> location, double realValue) {
  auto l = make_nn_shared<Literal>(location);
  l->realValue = realValue;
  l->type = Literal::Type::REAL;

  return l;
}

inline NotNullSharedPtr<Literal> makeLiteralBool(std::optional<NodeLocation> location, bool boolValue) {
  auto l = make_nn_shared<Literal>(location);
  l->boolValue = boolValue;
  l->type = Literal::Type::BOOLEAN;

  return l;
}

inline NotNullSharedPtr<Literal> makeLiteralNil(std::optional<NodeLocation> location) {
  auto l = make_nn_shared<Literal>(location);
  l->type = Literal::Type::NULLVAL;

  return l;
}

NotNullSharedPtr<InterpreterNode> null_node();

