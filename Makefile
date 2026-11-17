TOP = testbench
FILELIST = filelist.f
OBJ_DIR = obj_dir
WAVE = wave.vcd

VERILATOR = verilator
# VER_FLAGS = --binary --trace -Wall -f $(FILELIST)
VER_FLAGS = --binary --trace -Wno-fatal -f $(FILELIST)
# VER_FLAGS = --binary -DEBUG_PRINTS --trace -Wno-fatal -f $(FILELIST) 
# VER_FLAGS = --binary --trace --x-assign unique --x-initial unique -Wno-fatal -f $(FILELIST)

FW_DIR = fw
FW_HEX = $(FW_DIR)/fw.hex

.PHONY: all fw sim wave clean clean_sim build_project_config test_uart_serial

all: build_project_config fw clean_sim sim wave

build_project_config:
	python3 build_project_config.py

fw:
	$(MAKE) -C $(FW_DIR)

sim: $(FW_HEX)
	$(VERILATOR) $(VER_FLAGS)
	./$(OBJ_DIR)/V$(TOP)

test_uart_serial:
	$(VERILATOR) $(VER_FLAGS)
	bash fw/tools/run_test.sh test_uart_serial PASS TARGET=test_uart_serial SRC=drivers/src/uart1.c

wave:
	gtkwave $(WAVE)

clean:
	rm -rf $(OBJ_DIR) *.vcd *.fst
	$(MAKE) -C $(FW_DIR) clean

clean_sim:
	rm -rf $(OBJ_DIR) *.vcd *.fst
