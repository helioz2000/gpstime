TARGET = gpstime
BINDIR = /usr/sbin/
INITDIR = /etc/init.d/
INITEXT = .init
INITFILE = $(TARGET)$(INITEXT)
CFGDIR = /etc/
CFGEXT = .cfg
CFGFILE = $(TARGET)$(CFGEXT)
CRONDIR = /etc/cron.d/
CRONEXT = .cron
CORNELL = $(TARGET)$(CRONEXT)
LIBS = -lstdc++
CC = gcc
CFLAGS = -g -Wall -Wno-unused -Wno-unknown-pragmas

.PHONY: default all celan

default: $(TARGET)
all: default

SRC = $(wildcard *.cpp)
HDR = $(wildcard *.h)
OBJ = $(SRC:%.c=%.o)

#OBJECTS = $(patsubst %c, %o, $(wildcard *.c))
#HEADERS = $(wildcard *.h)
MODULEDIR = ../hardware/
MODULES =  

$(TARGET): $(OBJ)
#	make -C $(MODULEDIR)
	$(CC) $(OBJ) $(MODULES) -Wall $(LIBS) -o $@
#	$(CC) $(OBJ) -Wall $(LIBS) -o $@

%.o: %.c $(HDR)
	@echo $(SRC)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJ)

install:
	install -m 755 -o root $(TARGET) $(BINDIR)$(TARGET)
	install -m 644 -o root $(CRONFILE) $(CRONDIR)$(CRONFILE)
	mv $(CRONDIR)$(CRONFILE) $(CRONDIR)$(TARGET)
	install -m 755 -o root $(INITFILE) $(INITDIR)$(INITFILE)
	mv $(INITDIR)$(INITFILE) $(INITDIR)$(TARGET)
	@echo ++++++++++++++++++++++++++++++++++++++++++++
	@echo ++ Files have been installed
	@echo ++++++++++++++++++++++++++++++++++++++++++++
	@echo ++ If you want to start $(TARGET) on boot
	@echo ++ run the following command:
	@echo ++ sudo update-rc.d $(TARGET) defaults
	@echo ++------------------------------------------
	@echo ++ If you want to run $(TARGET) at regular
	@echo ++ intervals run the following command:
	@echo ++ sudo service cron reload

clean:
	-rm -f *.o
#	-rm -rf $(TARGET)

.PHONY : clean install
