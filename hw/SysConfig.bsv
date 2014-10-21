typedef 8 NumInputs;
typedef 8 StimSize;

typedef 11 StimMemAddrSize;

typedef 100    CyclesResolution;
typedef 200000 CyclesStimRate;

typedef 15 Log2LedPersistence;
typedef 7  Log2ErrorBlink;

// `NumInput` flags for displaying input pulses,
// 1 flag for the "stimuli update" pulse, and
// 1 flag for "word was matched"
typedef TAdd#(2, NumInputs) NumFlags;

Integer stimMemAddrSize = valueOf(StimMemAddrSize);
typedef Bit#(StimMemAddrSize) StimMemAddr;
typedef Bit#(StimSize) Stim;

typedef TAdd#(StimMemAddrSize, 1) AvalonAddrSize;
typedef 32                    AvalonDataSize;

typedef Bit#(AvalonDataSize) Word;
typedef Word TimeStamp;
