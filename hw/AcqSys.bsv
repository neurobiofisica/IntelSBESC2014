import AvalonSlave::*;
import InterruptSender::*;

typedef 3  AvalonAddrSize;
typedef 32 AvalonDataSize;

interface AcqSys;
	(* prefix="" *)
	interface InterruptSenderWires irqWires;
	(* prefix="" *)
	interface AvalonSlaveWires#(AvalonAddrSize, AvalonDataSize) avalonWires;
	(* always_ready, prefix="", enable="button" *)
	method Action button;
endinterface

(* synthesize *)
module mkAcqSys(AcqSys);
	InterruptSender irq <- mkInterruptSender;
	AvalonSlave#(AvalonAddrSize, AvalonDataSize) avalon <- mkAvalonSlave;

	rule handle_cmd;
		let cmd <- avalon.busClient.request.get;
		case (cmd) matches
			tagged AvalonRequest{addr: 0, data: .x, command: Write}:
				irq.enabled <= x != 0;
			tagged AvalonRequest{addr: 1, data: .*, command: Write}:
				irq.clear;
		endcase
	endrule

	interface irqWires = irq.irqWires;
	interface avalonWires = avalon.slaveWires;

	method Action button = irq.send;
endmodule
