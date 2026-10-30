BUILD_DIR ?= build
JOBS ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

.PHONY: all clean distclean

all: $(BUILD_DIR)/Makefile
	@$(MAKE) -C $(BUILD_DIR) -j$(JOBS)

$(BUILD_DIR)/Makefile:
	@cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug

clean:
	@if [ -d "$(BUILD_DIR)" ]; then $(MAKE) -C $(BUILD_DIR) clean; fi
	@rm -f c1 c1.exe

distclean:
	@rm -rf $(BUILD_DIR) c1 c1.exe *.dll
