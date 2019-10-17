all: clitest

clean:
	-rm clitest

clitest: Makefile file.c
	cc -o clitest file.c

test: clitest
	./clitest
