$(info ~~~~~~~~~~~~)
$(info ARGUMENT DEBUG : $(DEBUG))
$(info ~~~~~~~~~~~~)

ifeq ($(DEBUG),)
    # Define a default value for VARNAME if not defined
    DEBUG = 3
endif

CC = gcc
LDD = gcc

CFLAGS = -Wextra -DFWRITE -O0 -DOPTDEBUG=$(DEBUG) 
#CFLAGS = -Wextra -O0
SCFLAGS =-fPIC -DOPTDEBUG =$(DEBUG) 

#LDFLAGS=-static -lc   
#LDFLAGS=-Wl,-z,norelro -static -lc -lm 
LDFLAGS=


#TEST_CFLAGS=-Wextra -O0 -DOPTDEBUG -g 
#TEST_CFLAGS=-DOPTDEBUG -Wextra -O0 -g 
TEST_CFLAGS=-Wextra -O0 -DOPTDEBUG=$(DEBUG) 

#TEST_LDFLAGS=-Ttext=0x0800000
#TEST_LDFLAGS=-Wextra -O0 -DOPTDEBUG -g -static 
#TEST_LDFLAGS=-static -Wl,--image-base,0x8000000
TEST_LDFLAGS=-DOPTDEBUG

SLD_FLAGS=-shared -lc -ldl

INC_DIR = inc
SRC_DIR = src
LIB_DIR = lib

SINC_DIR = sinc
SSRC_DIR = ssrc
SLIB_DIR = slib

APP_DIR = app
OBJ_DIR = obj
BIN_DIR = bin

TEST_DIR = test
TEST_OBJ_DIR = test_obj
TEST_BIN_DIR = test_bin

LATEX_DIR = latex

SRCS = $(wildcard $(SRC_DIR)/*.c)
$(info SRCS : $(SRCS))

SSRCS = $(wildcard $(SSRC_DIR)/*.c)
$(info SSRCS : $(SSRCS))

APPS = $(wildcard $(APP_DIR)/*.c) 
$(info APPS : $(APPS))

TESTS = $(wildcard $(TEST_DIR)/*.c)
$(info TESTS : $(TESTS))

LIBS = $(patsubst $(SRC_DIR)/%.c,$(LIB_DIR)/%.o,$(SRCS))
$(info LIBS : $(LIBS))

SLIBS = $(patsubst $(SSRC_DIR)/%.c,$(SLIB_DIR)/%.so,$(SSRCS))
$(info SLIBS : $(SLIBS))

OBJS = $(patsubst $(APP_DIR)/%.c,$(OBJ_DIR)/%.o,$(APPS))
$(info OBJS : $(OBJS))

TEST_OBJS = $(patsubst $(TEST_DIR)/%.c,$(TEST_OBJ_DIR)/%.o,$(TESTS))
$(info TEST_OBJS : $(TEST_OBJS))

BINS = $(patsubst $(OBJ_DIR)/%.o,$(BIN_DIR)/%,$(OBJS))
$(info BINS : $(BINS))

TEST_BINS = $(patsubst $(TEST_OBJ_DIR)/%.o,$(TEST_BIN_DIR)/%,$(TEST_OBJS))
$(info TEST_BINS : $(TEST_BINS))

all: $(LIB_DIR) $(SLIB_DIR) $(OBJ_DIR) $(TEST_OBJS) $(BIN_DIR) $(TEST_BIN_DIR) $(TEST_BINS)  $(BINS) 

$(TEST_BIN_DIR)/%: $(LIBS) $(TEST_OBJ_DIR)/%.o 
	$(LDD) $(TEST_LDFLAGS) $(TEST_CFLAGS) -I$(INC_DIR) -o $@ $^ -L/usr/lib -lc -lm -luring

$(BIN_DIR)/%: $(LIBS) $(OBJ_DIR)/%.o 
	$(CC) $(CFLAGS) $(LDFLAGS) -I$(INC_DIR) -o $@ $^ -luring 

$(LIB_DIR)/%.o: $(SRC_DIR)/%.c 
	$(CC) $(CFLAGS) -I$(INC_DIR) -c -o $@ $<

$(SLIB_DIR)/%.so: $(SSRC_DIR)/%.c 
	$(CC) $(CFLAGS) $(SCFLAGS) -I$(INC_DIR) -o $@ $< $(SLD_FLAGS)

$(OBJ_DIR)/%.o: $(APP_DIR)/%.c 
	$(CC) $(CFLAGS) -I$(INC_DIR) -c -o $@ $<

$(TEST_OBJ_DIR)/%.o: $(TEST_DIR)/%.c 
	$(CC) $(TEST_CFLAGS) -I$(INC_DIR) -c -o $@ $<

$(BIN_DIR):
	mkdir $(BIN_DIR)
	
$(TEST_BIN_DIR): 
	mkdir $(TEST_BIN_DIR)

$(TEST_OBJ_DIR):
	mkdir $(TEST_OBJ_DIR)

$(OBJ_DIR):
	mkdir $(OBJ_DIR)

$(LIB_DIR):
	mkdir $(LIB_DIR) 

$(SLIB_DIR):
	mkdir $(SLIB_DIR) 

clean:
	rm -f $(BIN_DIR)/* $(OBJ_DIR)/* $(LIB_DIR)/* $(TEST_OBJ_DIR)/* $(TEST_BIN_DIR)/*

latex: $(LATEX_DIR)  
	pdflatex main.tex -output-directory=$(LATEX_DIR)
