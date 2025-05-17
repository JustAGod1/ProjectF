BUILDDIR := objs

SOURCES := $(shell find . -name '*.cpp')
HEADERS := $(shell find . -name '*.hpp')

LEXER_CODE = ./parser/lexer.cpp
LEXER_HEADER = parser/lexer.hpp
LEXER_CONF = parser/lexer.l

PARSER_CODE = ./parser/parser.cpp
PARSER_HEADER = parser/parser.hpp
PARSER_CONF = parser/parser.y


SOURCES += $(filter-out $(SOURCES),$(LEXER_CODE))
SOURCES += $(filter-out $(SOURCES),$(PARSER_CODE))

OBJECTS := $(addprefix $(BUILDDIR)/,$(SOURCES:%.cpp=%.o))

CXXFLAGS := -std=c++20
CXXFLAGS += -g

$(LEXER_CODE): $(LEXER_CONF) $(LEXER_HEADER) $(PARSER_HEADER)
	flex -o $(LEXER_CODE) $(LEXER_CONF)

$(PARSER_HEADER) $(PARSER_CODE): $(PARSER_CONF)
	bison -o $(PARSER_CODE) $(PARSER_CONF)

gen_code: $(LEXER_CODE) $(PARSER_HEADER) $(PARSER_CODE)

.PHONY: gen_code

$(BUILDDIR)/%.o: %.cpp $(HEADERS) gen_code
	mkdir -p $(shell dirname -- $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) -I. -c $< -o $@


clear:
	rm -rf $(OBJECTS)
	rm -rf $(LEXER_CODE) $(PARSER_CODE) $(PARSER_HEADER)

projf: $(OBJECTS)
	g++ -o $@ $(OBJECTS)






