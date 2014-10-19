interface CycleCounter#(numeric type n);
	method Bool ticked;
endinterface

module mkCycleCounter(CycleCounter#(n));
	Reg#(Bit#(TLog#(n))) cycleCounter <- mkReg(0);
	let maxCycleCount = fromInteger(valueOf(n) - 1);

	(* fire_when_enabled, no_implicit_conditions *)
	rule cycleCounterUpdate;
		cycleCounter <=	cycleCounter == maxCycleCount ? 0 : cycleCounter + 1;
	endrule

	method Bool ticked = (cycleCounter == 0);
endmodule
