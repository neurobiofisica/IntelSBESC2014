typedef 6      NumInputs;
typedef 100    CyclesResolution;
typedef 200000 CyclesStimRate;

typedef 15 Log2LedPersistence;
typedef 7  Log2ErrorBlink;

// `NumInput` LEDs for displaying input pulses,
// 1 LED for the "stimuli update" pulse, and
// 1 LED for the "word was matched" flag
typedef TAdd#(2, NumInputs) NumLeds;
