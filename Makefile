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

.PHONY: all fw sim wave clean clean_sim build_project_config test_uart0 test_uart_serial test_dma_driver test_adc_driver test_drivers

all: build_project_config fw clean_sim sim wave

build_project_config:
	python3 build_project_config.py

fw:
	$(MAKE) -C $(FW_DIR)

sim: $(FW_HEX)
	$(VERILATOR) $(VER_FLAGS)
	./$(OBJ_DIR)/V$(TOP)

test_uart0:
	$(VERILATOR) $(VER_FLAGS)
	bash fw/tools/run_test.sh test_uart0 PASS TARGET=test_uart0
	python3 fw/tools/check_uart0.py fw/build/test_uart0.sim.log

test_uart_serial:
	$(VERILATOR) $(VER_FLAGS)
	bash fw/tools/run_test.sh test_uart_serial PASS TARGET=test_uart_serial SRC=drivers/src/uart1.c

test_dma_driver:
	$(VERILATOR) $(VER_FLAGS)
	bash fw/tools/run_test.sh test_dma_driver PASS TARGET=test_dma_driver SRC=drivers/src/dma.c

test_adc_driver:
	$(VERILATOR) $(VER_FLAGS)
	bash fw/tools/run_test.sh test_adc_driver PASS TARGET=test_adc_driver SRC=drivers/src/adc.c

test_drivers:
	$(VERILATOR) $(VER_FLAGS)
	bash fw/tools/run_test.sh test_drivers PASS TARGET=test_drivers

wave:
	gtkwave $(WAVE)

clean:
	rm -rf $(OBJ_DIR) *.vcd *.fst
	$(MAKE) -C $(FW_DIR) clean

clean_sim:
	rm -rf $(OBJ_DIR) *.vcd *.fst
