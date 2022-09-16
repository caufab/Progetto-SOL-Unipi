# 	=========================
#   Autore: Fabrizio Cau
#   Matricola: 508700
#   Corso A Informatica 
#
#   Makefile
#	=========================

# compilatore da usare
CC			=  gcc
# flags del gcc
CFLAGS      = -std=c99 -Wall

# directories
BIN	= ./bin/
INCLUDES	= ./include/
SRC	= ./src/
LIB	= ./lib/
LIBSOPT	= -Wl,-rpath,$(LIB) -L$(LIB)
ALLLIBS	= -lmylib -ldatastruct

LPT	= -lpthread

.PHONY: all test1 test2 clean
.SUFFIXES: .c .h

#generalizzazione della compilazione da .c a eseguibile
$(BIN)%: $(SRC)%.c
	$(CC) $(CFLAGS) -I$(INCLUDES) -o $@ $<

#generalizzazione della compilazione da .c a .o
$(LIB)%.o: $(SRC)%.c
	$(CC) $(CFLAGS) -I$(INCLUDES) -c -o $@ $<

all	: clean $(BIN)supermercato $(BIN)direttore

$(BIN)direttore: $(SRC)direttore.c $(LIB)libmylib.so
	@echo "Compiling direttore"
	@$(CC) $(CFLAGS) -I$(INCLUDES) $< -o $@ $(LIBSOPT) -lmylib $(LPT)

$(BIN)supermercato: $(SRC)supermercato.c $(LIB)libmylib.so $(LIB)libdatastruct.so
	@echo "Compiling supermercato"
	@$(CC) $(CFLAGS) -I$(INCLUDES) $< -o $@ $(LIBSOPT) $(ALLLIBS) $(LPT)

$(LIB)libmylib.so: $(SRC)mylib.c $(INCLUDES)mylib.h 
	@echo "Compiling shared library mylib"
	@$(CC) $(CFLAGS) -I$(INCLUDES) $< -c -fPIC -o $(LIB)mylib.o
	@$(CC) -shared -o $@ $(LIB)mylib.o

$(LIB)libdatastruct.so: $(SRC)datastruct.c $(INCLUDES)datastruct.h 
	@echo "Compiling shared library datastruct"
	@$(CC) $(CFLAGS) -I$(INCLUDES) $< -c -fPIC -o $(LIB)datastruct.o
	@$(CC) -shared -o $@ $(LIB)datastruct.o

test1: all
	@chmod 755 -R ./
	@echo "Running direttore and supermercato and waiting 15 sec"
	@$(BIN)direttore &
	@(valgrind  --leak-check=full $(BIN)supermercato K 2 C 20 E 5 T 500 P 80 CUST_MOVES_TIMEOUT 30) &
	@sleep 15
	@echo "Sending SIGQUIT signal"
	@killall -w --signal SIGQUIT direttore

test2: all
	@chmod 755 -R ./
	@echo "Running direttore and waiting 25 sec"
	@($(BIN)direttore FORK_FROM_MNG 1) &
	@sleep 25
	@echo "Sending SIGHUP signal"
	@killall -w --signal SIGHUP direttore
	@./analisi.sh

clean: 
	@echo "Cleaning..."
	@rm -f $(BIN)supermercato
	@rm -f $(LIB)*
	@rm -f logsupermercato.txt
	@rm -f socket
