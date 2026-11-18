# Testy na PC: make (automat aj protokol USART)
CC ?= gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -I../Core/Inc

all: test_traffic_fsm test_traffic_serial

test_traffic_fsm: test_traffic_fsm.c ../Core/Src/traffic_fsm.c ../Core/Inc/traffic_fsm.h ../Core/Inc/traffic_phase.h
	$(CC) $(CFLAGS) test_traffic_fsm.c ../Core/Src/traffic_fsm.c -o $@
	./$@

test_traffic_serial: test_traffic_serial.c ../Core/Src/traffic_serial.c ../Core/Inc/traffic_serial.h
	$(CC) $(CFLAGS) test_traffic_serial.c ../Core/Src/traffic_serial.c -o $@
	./$@

clean:
	rm -f test_traffic_fsm test_traffic_fsm.exe test_traffic_serial test_traffic_serial.exe

.PHONY: all clean