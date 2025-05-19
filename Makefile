BUILDDIR := objs

SRC_DIR := src

SOURCES := $(shell find $(SRC_DIR) -name '*.cpp')
HEADERS := $(shell find $(SRC_DIR) -name '*.hpp')

LEXER_CODE = src/parser/lexer.cpp
LEXER_HEADER = src/parser/lexer.hpp
LEXER_CONF = src/parser/lexer.l

PARSER_CODE = src/parser/parser.cpp
PARSER_HEADER = src/parser/parser.hpp
PARSER_CONF = src/parser/parser.y

THIRD_PARTY_DIR := third_party

FMT_LIB_PATH = $(THIRD_PARTY_DIR)/fmt
POINTERS_LIB_PATH = $(THIRD_PARTY_DIR)/GSL

SOURCES += $(filter-out $(SOURCES),$(LEXER_CODE))
SOURCES += $(filter-out $(SOURCES),$(PARSER_CODE))

HEADERS += $(filter-out $(HEADERS),$(PARSER_HEADER))

OBJECTS := $(addprefix $(BUILDDIR)/,$(SOURCES:%.cpp=%.o))

CXXFLAGS := -std=c++20
CXXFLAGS += -g
CXXFLAGS += -O0

CPPFLAGS := -I$(SRC_DIR)
CPPFLAGS += -I$(FMT_LIB_PATH)/include
CPPFLAGS += -I$(POINTERS_LIB_PATH)/include
CPPFLAGS += -DFMT_HEADER_ONLY

#LDFLAGS := -L$(FMT_LIB_PATH)
LDFLAGS += -lfmt

$(LEXER_CODE) $(PARSER_HEADER) $(PARSER_CODE): $(LEXER_CONF) $(PARSER_CONF)
	bison -o $(PARSER_CODE) $(PARSER_CONF)
	flex -o $(LEXER_CODE) $(LEXER_CONF)

gen_code: $(LEXER_CODE) $(PARSER_HEADER) $(PARSER_CODE)

.PHONY: gen_code

$(BUILDDIR)/%.o: %.cpp $(HEADERS)
	mkdir -p $(shell dirname -- $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@


clear:
	rm -rf $(OBJECTS)
	rm -rf $(LEXER_CODE) $(PARSER_CODE) $(PARSER_HEADER)

projf: $(OBJECTS)
	g++ $(LDFLAGS) -o $@ $(OBJECTS)






