BSCFLAGS=-no-lift -aggressive-conditions
BSCHARDFLAGS=-opt-undetermined-vals -unspecified-to X
all:
	mkdir -p obj
	bsc -u -bdir obj -show-schedule -verilog $(BSCFLAGS) $(BSCHARDFLAGS) AcqSys.bsv
clean:
	rm -rf obj
	rm -f *.v *.sched
