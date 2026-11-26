# Testy na PC: make (prelozi a spusti testy automatu aj protokolu USART)
CC ?= gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -I../Core/Inc

TESTS = test_traffic_fsm test_traffic_serial

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

clean:
	rm -f test_traffic_fsm test_traffic_fsm.exe test_traffic_serial test_traffic_serial.exe

.PHONY: all test clean