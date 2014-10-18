interface InterruptSenderWires;
	(* always_ready, prefix="", result="ins" *)
	method Bit#(1) ins;
endinterface

interface InterruptSender;
	interface WriteOnly#(Bool) enabled;
	method Action send;
	method Action clear;

	interface InterruptSenderWires irqWires;
endinterface

module mkInterruptSender(InterruptSender);
	Array#(Reg#(Bool)) irqSet <- mkCReg(2,False);
	Reg#(Bool) irqEnabled <- mkReg(False);

	interface WriteOnly enabled;
		method Action _write(Bool x) = irqEnabled._write(x);
	endinterface

	method Action send;
		irqSet[1] <= True;
	endmethod
	method Action clear;
		irqSet[0] <= False;
	endmethod

	interface InterruptSenderWires irqWires;
		method Bit#(1) ins = irqSet[1] ? 1 : 0;
	endinterface
endmodule


