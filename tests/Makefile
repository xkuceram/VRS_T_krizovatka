# Testy stavoveho automatu na PC (simulovany cas): make
CC ?= gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -I../Core/Inc

test_traffic_fsm: test_traffic_fsm.c ../Core/Src/traffic_fsm.c ../Core/Inc/traffic_fsm.h ../Core/Inc/traffic_phase.h
	$(CC) $(CFLAGS) test_traffic_fsm.c ../Core/Src/traffic_fsm.c -o $@
	./$@

clean:
	rm -f test_traffic_fsm test_traffic_fsm.exe

.PHONY: clean
