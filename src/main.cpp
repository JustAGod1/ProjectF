#include "fmt/base.h"
#include "parser/parser.hpp"
#include "parser/lexer.hpp"
#include <deque>
#include <fstream>
#include <codecvt>
#include <sstream>
#include <string_view>

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cout << argv[0] << " <file>" << std::endl;
    return 1;
  }
  std::string_view filename{argv[1]};

  
  std::ifstream input{argv[1]};
  std::stringstream buf;
  buf << input.rdbuf();


  std::string source = buf.str();
  std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
  String converted = converter.from_bytes(source);
  Scanner s{converted};

  NotNullSharedPtr<Program> p = make_nn_shared<Program>(std::nullopt, std::vector<NotNullSharedPtr<Element>>{});
  yy::parser parser{s, p};

  int code = parser.parse();

  if (code != 0) {
    std::cout << "Parsing failed: " << code << std::endl;
    return 1;
  }

  //p->print(std::cout);

  Interpreter interpreter{converted};

  auto result = p->evaluate(p, interpreter, std::deque<InterpreterNodePtr>{});

  if (result.is_ok()) {
    result.get_value()->print(std::cout);
  } else if (result.is_ret()) {
    result.get_return()->print(std::cout);
  } else {
    std::cout << "exited with break";
  }

  std::cout << std::endl;

  return 0;
}
