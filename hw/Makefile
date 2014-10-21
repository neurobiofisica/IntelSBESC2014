BSCFLAGS=-no-lift -aggressive-conditions
BSCHARDFLAGS=-opt-undetermined-vals -unspecified-to X
all:
	mkdir -p obj
	bsc -u -bdir obj -show-schedule -verilog $(BSCFLAGS) $(BSCHARDFLAGS) AcqSys.bsv
	cp mkAcqSys.v PCIe_AvalonBSV/synthesis/submodules/
clean:
	rm -rf obj
	rm -f mkAcqSys.v *.sched
