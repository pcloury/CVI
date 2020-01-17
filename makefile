# makefile pour MinGW

CCOPT = -Wall -O2
C_SRC = kit_01w.c window_data.c wav_head.c quantizer.c a44.c
CPP_SRC =
EXE = a44.exe

OBJS = $(C_SRC:.c=.o) $(CPP_SRC:.cpp=.o)

# linkage
$(EXE) : $(OBJS)
	g++ -o $(EXE) $(OBJS)

# compilage
.c.o :
	gcc $(CCOPT) -c $<

.cpp.o :
	g++ $(CCOPT) -c $<
# other

clean :
	rm *.o; rm *.exe

# dependances : 
kit_01.o : kit_01.h
wav_head.o : wav_head.h
a42.o : kit_01w.h quantizer.h wav_head.h
