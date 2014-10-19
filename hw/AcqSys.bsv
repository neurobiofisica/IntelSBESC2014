import AvalonSlave::*;
import InterruptSender::*;
import JtagGetPut::*;

typedef 3  AvalonAddrSize;
typedef 32 AvalonDataSize;

interface AcqSys;
	(* prefix="" *)
	interface InterruptSenderWires irqWires;
	(* prefix="" *)
	interface AvalonSlaveWires#(AvalonAddrSize, AvalonDataSize) avalonWires;
	(* prefix="", always_ready, always_enabled *)
	method Action setButton((*port="button"*)Bit#(1) x);
endinterface

(* synthesize, clock_prefix="clk", reset_prefix="reset_n" *)
module mkAcqSys(AcqSys);
	InterruptSender irq <- mkInterruptSender;
	AvalonSlave#(AvalonAddrSize, AvalonDataSize) avalon <- mkAvalonSlave;

	Reg#(Bit#(1)) btnreg <- mkReg(1);
	Reg#(Bit#(1)) btnprevreg <- mkReg(1);

	rule handle_cmd;
		let cmd <- avalon.busClient.request.get;
		case (cmd) matches
			tagged AvalonRequest{addr: 0, data: .x, command: Write}:
				irq.enabled <= x != 0;
			tagged AvalonRequest{addr: 1, data: .*, command: Write}:
				irq.clear;
			tagged AvalonRequest{addr: .*, data: .*, command: Read}:
				avalon.busClient.response.put(32'hBADC0FFE);
		endcase
	endrule

	rule edge_detect (btnprevreg == 1 && btnreg == 0);
		irq.send;
	endrule

	interface irqWires = irq.irqWires;
	interface avalonWires = avalon.slaveWires;

	method Action setButton(Bit#(1) x);
		btnreg <= x;
		btnprevreg <= btnreg;
	endmethod
endmodule
