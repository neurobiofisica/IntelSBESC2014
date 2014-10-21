import SysConfig::*;
import Vector::*;

export Vector::*;
export StatusLEDWires(..);
export StatusErrSet(..);
export StatusLED(..);
export mkStatusLED;

interface StatusLEDWires;
	(* always_ready, result="LED" *)
	method Bit#(NumFlags) led;
endinterface

interface StatusErrSet;
	(* always_ready *)
	method Action set;
endinterface

interface StatusLED#(numeric type errports);
	interface StatusLEDWires wires;
	interface Vector#(errports, StatusErrSet) errorCondition;
	method Action errorClear;
endinterface

module mkStatusLED#(Bit#(NumFlags) syncedIn, Bool counterTicked) (StatusLED#(errports));
	Reg#(Bit#(Log2LedPersistence)) ledPersistenceCounter <- mkReg(?);
	Reg#(Bit#(Log2ErrorBlink)) ledBlinkCounter <- mkReg(?);
	Bool isLedBlink = ledBlinkCounter[valueOf(Log2ErrorBlink)-1] == 1'b1;

	let errPorts = valueOf(errports);
	Array#(Reg#(Bool)) errorCond <- mkCReg(errPorts+1, False);

	Array#(Reg#(Bit#(NumFlags))) ledFlags <- mkCReg(2, 0);
	Reg#(Bit#(NumFlags)) ledReg <- mkReg(~0);

	(* fire_when_enabled, no_implicit_conditions *)
	rule setInputs;
		ledFlags[1] <= ledFlags[1] | syncedIn;
	endrule

	(* fire_when_enabled, no_implicit_conditions *)
	rule counterTick(counterTicked);
		ledPersistenceCounter <= ledPersistenceCounter + 1;
		if(ledPersistenceCounter == 0) begin
			ledBlinkCounter <= ledBlinkCounter + 1;
			ledReg <= errorCond[0] ?
				(isLedBlink ? 0 : ~0) :
				ledFlags[0];
			ledFlags[0] <= 0;
		end
	endrule

	function genStatusErrSet(i);
		return (interface StatusErrSet;
			method Action set;
				errorCond[i] <= True;
			endmethod
		endinterface);
	endfunction

	interface Vector errorCondition = genWith(genStatusErrSet);

	method Action errorClear;
		errorCond[errPorts] <= False;
	endmethod

	interface StatusLEDWires wires;
		method Bit#(NumFlags) led = ledReg;
	endinterface
endmodule
