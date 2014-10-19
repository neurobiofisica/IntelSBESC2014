module AsyncPulseSync(clk, in, out);
	input clk, in;
	output out;
	reg stageA, stageB, dedup;

	assign out = stageB & ~dedup;

	always@(posedge clk) begin
		stageA <= in;
		stageB <= stageA;
		dedup  <= stageB;
	end

	initial
	begin
		stageA = 1'b0;
		stageB = 1'b0;
		dedup  = 1'b0;
	end
endmodule

