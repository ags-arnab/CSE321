# Makefile for CSE321 Assignment 3
# Compile both tasks

CC = gcc
CFLAGS = -Wall -Wextra
TARGETS = p1 p2

all: $(TARGETS)

p1: p1.c
	$(CC) $(CFLAGS) -o p1 p1.c

p2: p2.c
	$(CC) $(CFLAGS) -o p2 p2.c

clean:
	rm -f $(TARGETS)

test1: p1
	@echo "Testing Task 1 - Banking Application"
	@echo "Test 1: Add Money (valid amount)"
	@printf "a\n100\n" | ./p1
	@echo ""
	@echo "Test 2: Add Money (invalid amount)"
	@printf "a\n-100\n" | ./p1
	@echo ""
	@echo "Test 3: Withdraw Money (valid amount)"
	@printf "w\n600\n" | ./p1
	@echo ""
	@echo "Test 4: Withdraw Money (invalid amount)"
	@printf "w\n-300\n" | ./p1
	@echo ""
	@echo "Test 5: Check Balance"
	@printf "c\n" | ./p1
	@echo ""
	@echo "Test 6: Invalid Selection"
	@printf "x\n" | ./p1

test2: p2
	@echo "Testing Task 2 - Office Management Platform"
	@echo "Test 1: Valid workspace"
	@echo "cse321" | ./p2
	@echo ""
	@echo "Test 2: Invalid workspace"
	@echo "abc" | ./p2

test: test1 test2

.PHONY: all clean test test1 test2