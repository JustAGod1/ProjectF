#include <memory>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include "interpreter/interpreter.hpp"
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
    virtual ~ASTNode() = default;
};

// Element can be Atom, Literal, or List
class Element : public ASTNode {
public:
    virtual ~Element() = default;
};

// Identifier: sequence of letters and digits
class Identifier : public ASTNode {
public:
    std::string name;

    Identifier(std::string s) : name(std::move(s)) {
    }

    void print(std::ostream& out, int indent = 0) const override {
        out << std::string(indent, ' ') << "Identifier: " << name << "\n";
    }
    EvaluationResult<InterpreterNodePtr> evaluate(
        InterpreterNodePtr self,
        Interpreter& Interpreter
      ) const override;
};



// Program: collection of Elements
class Program : public ASTNode {
public:
    std::vector<std::shared_ptr<Element>> elements;
    
    void print(std::ostream& out, int indent = 0) const override {
        out << std::string(indent, ' ') << "Program:\n";
        for (const auto& el : elements) {
            el->print(out, indent + 2);
        }
    }
    EvaluationResult<InterpreterNodePtr> evaluate(
        InterpreterNodePtr self,
        Interpreter& Interpreter
      ) const override;
};

// List: ( elements... )
class List : public Element {
public:
    std::vector<InterpreterNodePtr> elements;

    List(std::vector<InterpreterNodePtr> elements) : elements(std::move(elements)) {}

    void print(std::ostream& out, int indent = 0) const override {
        out << std::string(indent, ' ') << "List:\n";
        for (const auto& el : elements) {
            el->print(out, indent + 2);
        }
    }
    EvaluationResult<InterpreterNodePtr> evaluate(
        InterpreterNodePtr self,
        Interpreter& Interpreter
      ) const override;
};

// Atom: Identifier
class Atom : public Element {
public:
    std::shared_ptr<Identifier> identifier;

    Atom(std::shared_ptr<Identifier> identifier) : identifier(identifier) {}
    
    void print(std::ostream& out, int indent = 0) const override {
        out << std::string(indent, ' ') << "Atom: ";
        identifier->print(out, 0);
    }

    EvaluationResult<InterpreterNodePtr> evaluate(
        InterpreterNodePtr self,
        Interpreter& Interpreter
      ) const override;
};

// Literal values
class Literal : public Element {
public:
    enum class Type { INTEGER, REAL, BOOLEAN, NULLVAL };
    Type type;
    
    union {
        int intValue;
        double realValue;
        bool boolValue;
    };
    
    void print(std::ostream& out, int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "Literal: ";
        switch (type) {
            case Type::INTEGER: std::cout << intValue << "i"; break;
            case Type::REAL: std::cout << realValue << "d"; break;
            case Type::BOOLEAN: std::cout << (boolValue ? "true" : "false"); break;
            case Type::NULLVAL: std::cout << "null"; break;
        }
        std::cout << "\n";
    }

    EvaluationResult<InterpreterNodePtr> evaluate(
        InterpreterNodePtr self,
        Interpreter& Interpreter
      ) const override;

    bool less(const Literal& literal) const;
    bool eq(const Literal& literal) const { return !less(literal) && !literal.less(*this); }
    bool neq(const Literal& literal) const { return !eq(literal); }
    bool lesseq(const Literal& literal) const { return less(literal) || eq(literal); }
    bool greater(const Literal& literal) const { return !less(literal) && !eq(literal);}
    bool greatereq(const Literal& literal) const { return greater(literal) || eq(literal); }
};

inline std::shared_ptr<Literal> makeLiteralInt(int intValue) {
  auto l = std::make_shared<Literal>();
  l->intValue = intValue;
  l->type = Literal::Type::INTEGER;

  return l;
}

inline std::shared_ptr<Literal> makeLiteralReal(double realValue) {
  auto l = std::make_shared<Literal>();
  l->realValue = realValue;
  l->type = Literal::Type::REAL;

  return l;
}

inline std::shared_ptr<Literal> makeLiteralBool(bool boolValue) {
  auto l = std::make_shared<Literal>();
  l->boolValue = boolValue;
  l->type = Literal::Type::BOOLEAN;

  return l;
}

inline std::shared_ptr<Literal> makeLiteralNil() {
  auto l = std::make_shared<Literal>();
  l->type = Literal::Type::NULLVAL;

  return l;
}

extern std::shared_ptr<InterpreterNode> NULL_NODE;

