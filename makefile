SRC=src
INC=includes

#defining wildcards to avoid typing all the files separately:
SOURCES= $(wildcard $(SRC)/*.cpp)
HEADERS= $(wildcard $(INC)/*.hpp)

# The 'all' target is the default one.
all: api-bomber 

api-bomber: main.cpp $(SOURCES) $(HEADERS) 
	g++ -I$(INC) main.cpp $(SOURCES) -o api-bomber -lcurl

clean:
	rm -f api-bomber

# The PHONY tag tells that 
# these targets are not actual files in our project,
#  important for things like 
# all, clean, (and if needed then test, install etc)
.PHONY: all clean