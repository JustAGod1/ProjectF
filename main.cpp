#include "parser/parser.hpp"
#include "parser/lexer.hpp"
#include <fstream>
#include <memory>
#include <string_view>

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cout << argv[0] << " <file>" << std::endl;
    return 1;
  }
  std::string_view filename{argv[1]};

  Scanner s{};
  std::ifstream input{argv[1]};
  s.switch_streams(input, std::cout);
  std::shared_ptr<Program> p = std::make_shared<Program>();
  yy::parser parser{s, p};

  int code = parser.parse();

  std::cout << "code: " << code << std::endl;

  //p->print(std::cout);

  Interpreter interpreter{};

  p->evaluate(p, interpreter);

  return 0;
}
