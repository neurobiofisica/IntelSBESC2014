import Clocks::*;
import Vector::*;

export SyncPulseIfc(..);
export mkAsyncPulseSync;

export Vector::*;
export AcqIn(..);
export toAcqIn;
export fromPulseVector;


import "BVI" AsyncPulseSync =
	module mkAsyncPulseSync(SyncPulseIfc);
		method send() enable(in);
		method out pulse();
		default_clock clk(clk, (*unused*)clk_gate);
		default_reset no_reset;
		schedule (pulse) CF (pulse);
		schedule (send) C (send);
		schedule (send) CF (pulse);
	endmodule


interface AcqIn;
	(* always_ready, enable="IN" *)
	method Action send;
endinterface

function AcqIn toAcqIn(SyncPulseIfc ifc);
	return (interface AcqIn;
		method Action send = ifc.send;
	endinterface);
endfunction

function Bit#(n) fromPulseVector(Vector#(n, SyncPulseIfc) vec);
	Bit#(n) res;
	for(Integer i = 0; i < valueOf(n); i = i + 1)
		res[i] = vec[i].pulse ? 1'b1 : 1'b0;
	return res;
endfunction
