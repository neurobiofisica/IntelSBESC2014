import FIFOF::*;
import BRAM::*;
import Connectable::*;
import SysConfig::*;
import AsyncPulseSync::*;
import StatusLED::*;
import CycleCounter::*;
import AvalonSlave::*;
import InterruptSender::*;

typedef Tuple2#(Bit#(NumFlags), TimeStamp) AcqFifoContents;

interface AcqSys;
	(* prefix="" *)
	interface InterruptSenderWires irqWires;
	(* prefix="" *)
	interface AvalonSlaveWires#(AvalonAddrSize, AvalonDataSize) avalonWires;
	(* prefix="" *)
	interface StatusLEDWires ledWires;
	(* prefix="CH" *)
	interface Vector#(NumInputs, AcqIn) inputs;
	(* prefix="", result="STIM" *)
	method Stim stimuli;
endinterface

(* synthesize, clock_prefix="clk", reset_prefix="reset_n" *)
module mkAcqSys(AcqSys);
	AvalonSlave#(AvalonAddrSize, AvalonDataSize) avalon <- mkAvalonSlave;

	Vector#(NumInputs, SyncPulseIfc) inSyncs <- replicateM(mkAsyncPulseSync);
	Bit#(NumInputs) syncedIn = fromPulseVector(inSyncs);

	CycleCounter#(CyclesStimRate) stimRate <- mkCycleCounter;
	Bit#(NumFlags) flagIn = {1'b0, stimRate.ticked?1'b1:1'b0, syncedIn};

	CycleCounter#(CyclesResolution) cycleCounter <- mkCycleCounter;
	StatusLED#(2) led <- mkStatusLED(flagIn, cycleCounter.ticked);
	Reg#(Bool) acqStarted <- mkReg(False);
	Reg#(TimeStamp) timestamp <- mkReg(0);
	Array#(Reg#(Bit#(NumFlags))) channelFlags <- mkCReg(2, 0);

	FIFOF#(AcqFifoContents) acqFifo <- mkSizedFIFOF(2*valueOf(NumFlags));
	RWire#(AcqFifoContents) acqFifoFirst <- mkRWire;
	FIFOF#(Stim) stimFifo <- mkFIFOF;

	BRAM2Port#(StimMemAddr, Stim) stimMem <- mkBRAM2Server(defaultValue);
	FIFOF#(void) stimMemPendRead <- mkFIFOF;  // enforce order on responses

	rule peekAcqFifo;
		acqFifoFirst.wset(acqFifo.first);
	endrule

	rule answerStimMemRead;
		let resp <- stimMem.portB.response.get;
		avalon.busClient.response.put(extend(resp));
		stimMemPendRead.deq;
	endrule

	rule handleCmd(!stimMemPendRead.notEmpty);
		let cmd <- avalon.busClient.request.get;
		(*split*)
		if (cmd.addr[stimMemAddrSize] == 1'b0) begin
			(*split*)
			case (cmd) matches
				// Register @0x00: Enable/disable acquisition.
				tagged AvalonRequest{addr: 0, data: .x, command: Write}:
					action
						acqStarted <= x != 0;
						(*split*)
						if(x == 0) begin
							led.errorClear;
							acqFifo.clear;
							stimFifo.clear;
						end
					endaction
				tagged AvalonRequest{addr: 0, data: .*, command: Read}:
					avalon.busClient.response.put(acqStarted ? 1 : 0);
				// Register @0x04: Read flags from the FIFO. MSB is zero if data is valid.
				tagged AvalonRequest{addr: 1, data: .*, command: Read}:
					action
						Bit#(NumFlags) flags = tpl_1(fromMaybe(tuple2(0,0), acqFifoFirst.wget));
						Bit#(TSub#(AvalonDataSize, 1)) extFlags = extend(flags);
						Bit#(1) invalidFlag = isValid(acqFifoFirst.wget) ? 0 : 1;
						avalon.busClient.response.put({invalidFlag, extFlags});
					endaction
				// Register @0x08: Read timestamp from the FIFO. WARNING: may block.
				tagged AvalonRequest{addr: 2, data: .*, command: Read}:
					action
						TimeStamp tstamp = tpl_2(acqFifo.first);
						avalon.busClient.response.put(tstamp);
						acqFifo.deq;
					endaction
					// Register @0x0c: Write sample to stimuli FIFO. WARNING: may block.
				tagged AvalonRequest{addr: 3, data: .x, command: Write}:
					stimFifo.enq(truncate(x));
				// Deal with reads to addresses not listed above
				tagged AvalonRequest{addr: .*, data: .*, command: Read}:
					avalon.busClient.response.put(32'hBADC0FFE);
			endcase
		end else begin
			if(cmd.command == Read)
				stimMemPendRead.enq(?);
			stimMem.portB.request.put(BRAMRequest{
				write: cmd.command == Write,
				address: truncate(cmd.addr),
				datain: truncate(cmd.data),
				responseOnWrite: False
			});
		end
	endrule

	(* fire_when_enabled *)
	rule stimLoad(acqStarted && stimRate.ticked);
		(*split*)
		if(stimFifo.notEmpty) begin
			stimFifo.deq;
		end else begin
			led.errorCondition[1].set;
		end
	endrule

	(* fire_when_enabled *)
	rule timestampUpdate(acqStarted && cycleCounter.ticked);
		let flags = asReg(channelFlags[0]);
		Bit#(TSub#(NumFlags,1)) truncatedFlags = truncate(flags);
		(*split*)
		if(truncatedFlags != 0) begin
			(*split*)
			if(acqFifo.notFull) begin
				acqFifo.enq(tuple2(flags, timestamp));
			end else begin
				led.errorCondition[0].set;  // TX FIFO overflow
			end
		end
		flags <= 0;
		timestamp <= timestamp + 1;
	endrule

	(* fire_when_enabled, no_implicit_conditions *)
	rule blendChannelFlags(acqStarted);
		channelFlags[1] <= channelFlags[1] | flagIn;
	endrule

	method Stim stimuli = acqStarted ? stimFifo.first : 0;

	interface irqWires = irqSender(acqFifo.notEmpty);
	interface avalonWires = avalon.slaveWires;
	interface ledWires = led.wires;
	interface inputs = Vector::map(toAcqIn, inSyncs);
endmodule
