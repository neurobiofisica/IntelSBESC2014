interface InterruptSenderWires;
	(* always_ready, prefix="", result="ins" *)
	method Bit#(1) ins;
endinterface

function InterruptSenderWires irqSender(Bool condition);
	return(interface InterruptSenderWires;
		method Bit#(1) ins = condition ? 1 : 0;
	endinterface);
endfunction
