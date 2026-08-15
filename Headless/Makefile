RUNTIME_CORE := ../Source/GV2RuntimeCore
CONTENT_CORE := ../Source/GV2ContentCore
LUA_DIR := $(RUNTIME_CORE)/Private/ThirdParty/Lua54
BUILD_DIR := build

CC ?= cc
CXX ?= c++
CFLAGS := -O2 -DNDEBUG -DLUA_USE_LINUX=1 -MMD -MP -I$(LUA_DIR)
CXXFLAGS := -O2 -DNDEBUG -std=c++20 -MMD -MP -I$(RUNTIME_CORE)/Public -I$(CONTENT_CORE)/Public -I$(LUA_DIR)

LUA_SOURCES := $(wildcard $(LUA_DIR)/*.c)
LUA_OBJECTS := $(patsubst $(LUA_DIR)/%.c,$(BUILD_DIR)/lua_%.o,$(LUA_SOURCES))
CONTENT_CORE_NAMES := BuildResult CanonicalHash DefinitionEnvelope Diagnostic ExtensionSchema FieldValidation GV2ContentCore Json5Conformance Json5Lexer Json5Parser PackageDescriptor PackageDiscovery ParseLimits PoParser PoParserConformance RepositoryBuilder RepositorySnapshot RepresentativeCore ScalarValidation SchemaRegistry SemanticValidation StableId Value
CONTENT_CORE_OBJECTS := $(addprefix $(BUILD_DIR)/content_,$(addsuffix .o,$(CONTENT_CORE_NAMES)))
CORE_OBJECTS := $(BUILD_DIR)/GV2RuntimeSession.o $(CONTENT_CORE_OBJECTS)
MAIN_OBJECT := $(BUILD_DIR)/main.o
TARGET := $(BUILD_DIR)/gv2-headless
DEPENDENCIES := $(LUA_OBJECTS:.o=.d) $(CORE_OBJECTS:.o=.d) $(MAIN_OBJECT:.o=.d)

-include $(DEPENDENCIES)

.PHONY: all self-test clean

all: $(TARGET)

self-test: $(TARGET)
	$(TARGET) --self-test

$(TARGET): $(LUA_OBJECTS) $(CORE_OBJECTS) $(MAIN_OBJECT)
	$(CXX) $^ -o $@ -lm -ldl

$(BUILD_DIR)/lua_%.o: $(LUA_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/GV2RuntimeSession.o: $(RUNTIME_CORE)/Private/GV2RuntimeSession.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/content_%.o: $(CONTENT_CORE)/Private/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(MAIN_OBJECT): Source/main.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(LUA_OBJECTS) $(CORE_OBJECTS) $(MAIN_OBJECT) $(TARGET)
