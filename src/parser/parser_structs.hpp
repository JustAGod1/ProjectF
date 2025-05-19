#include <memory>
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

using ASTNodePtr = std::shared_ptr<ASTNode>;


// Base AST Node class
class ASTNode : public InterpreterNode {
public:

  ASTNode(std::optional<NodeLocation> location) : InterpreterNode(location) {}

  virtual ~ASTNode() = default;
};

// Element can be Atom, Literal, or List
class Element : public ASTNode {
public:
  Element(std::optional<NodeLocation> location) : ASTNode(location) {}
  virtual ~Element() = default;
};

// Identifier: sequence of letters and digits
class Identifier : public ASTNode {
public:
    std::string name;

    Identifier(std::optional<NodeLocation> location, std::string name) : ASTNode(location), name(std::move(name)) {}

    void print(std::ostream& out, int indent = 0) const override {
        out << std::string(indent, ' ') << "Identifier: " << name << "\n";
    }
    EvaluationResult<InterpreterNodePtr> evaluate(
        InterpreterNodePtr self,
        Interpreter& Interpreter,
      std::deque<std::shared_ptr<InterpreterNode>> args
      ) const override;
};



// Program: collection of Elements
class Program : public ASTNode {
public:
    std::vector<std::shared_ptr<Element>> elements;

    Program(std::optional<NodeLocation> location,
        std::vector<std::shared_ptr<Element>> elements) : ASTNode(location), elements(std::move(elements)) {}
    
    void print(std::ostream& out, int indent = 0) const override {
        out << std::string(indent, ' ') << "Program:\n";
        for (const auto& el : elements) {
            el->print(out, indent + 2);
        }
    }
    EvaluationResult<InterpreterNodePtr> evaluate(
        InterpreterNodePtr self,
        Interpreter& Interpreter,
      std::deque<std::shared_ptr<InterpreterNode>> args
      ) const override;
};

// List: ( elements... )
class List : public Element {
public:
    std::vector<InterpreterNodePtr> elements;

    List(std::optional<NodeLocation> location, std::vector<InterpreterNodePtr> elements) : Element(location), elements(std::move(elements)) {}

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
      std::deque<std::shared_ptr<InterpreterNode>> args
      ) const override;
};

class Quote : public Element {
public:
    InterpreterNodePtr inner;

    Quote(std::optional<NodeLocation> location, InterpreterNodePtr inner) : Element(location), inner(std::move(inner)) {}

    void print(std::ostream& out, int indent = 0) const override {
        out << std::string(indent, ' ') << "'";
        inner->print(out, indent);
    }


    EvaluationResult<InterpreterNodePtr> evaluate(
        InterpreterNodePtr self,
        Interpreter& Interpreter,
      std::deque<std::shared_ptr<InterpreterNode>> args
      ) const override;
};

// Atom: Identifier
class Atom : public Element {
public:
    std::shared_ptr<Identifier> identifier;

    Atom(std::optional<NodeLocation> location, std::shared_ptr<Identifier> identifier) : Element(location), identifier(identifier) {}
    
    void print(std::ostream& out, int indent = 0) const override {
        out << std::string(indent, ' ') << "Atom: ";
        identifier->print(out, 0);
    }


    EvaluationResult<InterpreterNodePtr> evaluate(
        InterpreterNodePtr self,
        Interpreter& Interpreter,
      std::deque<std::shared_ptr<InterpreterNode>> args
      ) const override;
};

// Literal values
class Literal : public Element {
public:
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
        std::deque<std::shared_ptr<InterpreterNode>> args
      ) const override;

    bool less(const Interpreter& interpreter, const Literal& literal) const;
    bool eq(const Interpreter& interpreter, const Literal& literal) const { return !less(interpreter, literal) && !literal.less(interpreter, *this); }
    bool neq(const Interpreter& interpreter, const Literal& literal) const { return !eq(interpreter, literal); }
    bool lesseq(const Interpreter& interpreter, const Literal& literal) const { return less(interpreter, literal) || eq(interpreter, literal); }
    bool greater(const Interpreter& interpreter, const Literal& literal) const { return !less(interpreter, literal) && !eq(interpreter, literal);}
    bool greatereq(const Interpreter& interpreter, const Literal& literal) const { return greater(interpreter, literal) || eq(interpreter, literal); }
};

inline std::shared_ptr<Literal> makeLiteralInt(std::optional<NodeLocation> location, int intValue) {
  auto l = std::make_shared<Literal>(location);
  l->intValue = intValue;
  l->type = Literal::Type::INTEGER;

  return l;
}

inline std::shared_ptr<Literal> makeLiteralReal(std::optional<NodeLocation> location, double realValue) {
  auto l = std::make_shared<Literal>(location);
  l->realValue = realValue;
  l->type = Literal::Type::REAL;

  return l;
}

inline std::shared_ptr<Literal> makeLiteralBool(std::optional<NodeLocation> location, bool boolValue) {
  auto l = std::make_shared<Literal>(location);
  l->boolValue = boolValue;
  l->type = Literal::Type::BOOLEAN;

  return l;
}

inline std::shared_ptr<Literal> makeLiteralNil(std::optional<NodeLocation> location) {
  auto l = std::make_shared<Literal>(location);
  l->type = Literal::Type::NULLVAL;

  return l;
}

std::shared_ptr<InterpreterNode> null_node();

