PROG = interface                   # Program we are building
DELETE = rm -rf                   # Command to remove files
OUT = -o $(PROG)                 # Compiler argument for output file
CC = arm-mix410-linux-g++
CC3 = arm-hisiv500-linux-g++
CC4 = g++
#SOURCES = ./src/main.cpp ./src/mongoose.c ./src/configureInterface.cpp ./src/cJSON.c ./src/CJsonObject.cpp       # Source code files
SOURCES = ./src/*
CFLAGS = -std=c++11  -W -Wall -Wextra -g -I ./inc -l pthread # Build options


$(PROG): $(SOURCES)       # Build program from sources
	$(CC) $(SOURCES) $(CFLAGS) $(OUT)
#cp interface ./update/V0015R0001/update_app/server/micagent/server/addition/app/bin/interface

clean:                    # Cleanup. Delete built program and all build artifacts
	$(DELETE) $(PROG) *.o *.obj *.exe *.dSYM

