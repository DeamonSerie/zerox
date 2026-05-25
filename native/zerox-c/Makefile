CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Os
ROOT := ../..
OUT := $(ROOT)/.zerox/bin/zerox
NODE_TS := node --experimental-strip-types --disable-warning=ExperimentalWarning
BUILD_STAMP := $(OUT).build
HOST_ID := $(shell uname -s 2>/dev/null || echo unknown)-$(shell uname -m 2>/dev/null || echo unknown)
BUILD_ID := $(HOST_ID)|$(CC)|$(CFLAGS)
SRC := $(wildcard src/*.c)
INCLUDES := $(wildcard include/*.h) $(wildcard src/*.h) $(wildcard src/*.inc)
EMBEDDED_SKILLS := src/embedded_skills.inc
SKILL_INPUTS := $(ROOT)/skills/zerox/SKILL.md $(wildcard $(ROOT)/skill-data/*.md)

BOTAN_DIR := $(ROOT)/botan-master-master
BOTAN_LIB := $(BOTAN_DIR)/libbotan-3.a

.PHONY: all clean install-local botan-build embed-runtime FORCE

all: $(OUT)

$(BUILD_STAMP): FORCE
	@mkdir -p $(dir $(OUT))
	@current="$$(cat "$(BUILD_STAMP)" 2>/dev/null || true)"; \
	if [ "$$current" != "$(BUILD_ID)" ]; then \
		printf '%s\n' "$(BUILD_ID)" > $(BUILD_STAMP); \
	fi

$(EMBEDDED_SKILLS): $(ROOT)/scripts/embed-skill-data.mts $(SKILL_INPUTS)
	cd $(ROOT) && $(NODE_TS) scripts/embed-skill-data.mts

$(OUT): $(BUILD_STAMP) $(SRC) $(INCLUDES) $(EMBEDDED_SKILLS)
	@mkdir -p $(dir $(OUT))
	$(CC) $(CFLAGS) -Iinclude $(SRC) -o $(OUT)

embed-runtime:
	cd $(ROOT) && $(NODE_TS) scripts/embed-runtime-sources.mts

# Build Botan from source (static library).
# Botan is a C++ library; the compiler automatically detects the built
# library via bin/zerox (which sets ZEROX_BOTAN_DIR) or via pkg-config.
botan-build: $(BOTAN_LIB)

$(BOTAN_LIB): $(BOTAN_DIR)/configure.py
	cd $(BOTAN_DIR) && python3 configure.py --cc=gcc --disable-shared --enable-modules=sha2_32,sha2_64,aes,chacha,salsa20,poly1305,des,blowfish,twofish,serpent,camellia,rsa,ecc,pbkdf2,hmac,ed25519 --minimized-build
	$(MAKE) -C $(BOTAN_DIR)

clean:
	rm -f $(OUT) $(BUILD_STAMP)

install-local: $(OUT)
	@printf 'installed %s\n' "$(OUT)"
