# Testy na PC: make (prelozi a spusti testy automatu, protokolu USART a ich spoluprace)
CC ?= gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -I../Core/Inc

TESTS = test_traffic_fsm test_traffic_serial test_serial_fsm

all: test

# Spusti oba testy pri kazdom volani, aj ked sa binarky nemenili.
# Ak zlyha prvy, druhy sa aj tak spusti a make skonci chybou.
test: $(TESTS)
	@status=0; for t in $(TESTS); do ./$$t || status=1; done; exit $$status

# Preklad (bez spustenia)
test_traffic_fsm: test_traffic_fsm.c ../Core/Src/traffic_fsm.c ../Core/Inc/traffic_fsm.h ../Core/Inc/traffic_phase.h
	$(CC) $(CFLAGS) test_traffic_fsm.c ../Core/Src/traffic_fsm.c -o $@

test_traffic_serial: test_traffic_serial.c ../Core/Src/traffic_serial.c ../Core/Inc/traffic_serial.h
	$(CC) $(CFLAGS) test_traffic_serial.c ../Core/Src/traffic_serial.c -o $@

SERIAL_FSM_SRC = ../Core/Src/traffic_serial.c ../Core/Src/traffic_serial_link.c ../Core/Src/traffic_fsm.c
test_serial_fsm: test_serial_fsm.c $(SERIAL_FSM_SRC) ../Core/Inc/traffic_serial.h ../Core/Inc/traffic_serial_link.h ../Core/Inc/traffic_fsm.h
	$(CC) $(CFLAGS) test_serial_fsm.c $(SERIAL_FSM_SRC) -o $@

clean:
	rm -f test_traffic_fsm test_traffic_fsm.exe test_traffic_serial test_traffic_serial.exe test_serial_fsm test_serial_fsm.exe

.PHONY: all test clean