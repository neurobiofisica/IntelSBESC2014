interface ProgrammableCycleCounter#(numeric type w);
	method Reg#(Bit#(w)) period;
	method Bool ticked;
endinterface

module mkProgrammableCycleCounter#(Bit#(w) defv) (ProgrammableCycleCounter#(w));
	Reg#(Bit#(w)) cycleCounter <- mkReg(1);
	Reg#(Bit#(w)) curPeriod <- mkReg(defv);

	(* fire_when_enabled, no_implicit_conditions *)
	rule cycleCounterUpdate;
		cycleCounter <=	cycleCounter == curPeriod ? 1 : cycleCounter + 1;
	endrule

	interface period = curPeriod;
	method Bool ticked = (cycleCounter == 1);
endmodule
