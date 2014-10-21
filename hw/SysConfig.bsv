typedef 8 NumInputs;
typedef 8 StimSize;

typedef 100    CyclesResolution;
typedef 200000 CyclesStimRate;

typedef 15 Log2LedPersistence;
typedef 7  Log2ErrorBlink;

// `NumInput` flags for displaying input pulses,
// 1 flag for the "stimuli update" pulse, and
// 1 flag for "word was matched"
typedef TAdd#(2, NumInputs) NumFlags;
