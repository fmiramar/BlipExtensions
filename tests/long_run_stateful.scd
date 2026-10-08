(
// Opt-in ten-minute NRT state test for the three stateful classes.
var duration = 600;
var path = Platform.defaultTempDir +/+ "blip-extensions-long-run.wav";
var completed = FlowVar.new;
var options = ServerOptions.new;
var definition = SynthDef(\blipExtensionsLongRun, {
	Out.ar(0, [
		BlipWidth.ar(
			SinOsc.ar(0.013).range(-1000, 1000),
			LFPulse.kr(0.7).range(1, 256),
			SinOsc.ar(0.071).range(0.00001, 0.99999)
		),
		BlipAD.ar(
			SinOsc.ar(0.017).range(-100, 100),
			LFPulse.kr(0.9).range(1, 256),
			LFNoise1.kr(0.8).range(1 / SampleRate.ir, 0.02),
			LFNoise1.kr(0.8).range(0.03, 0.4)
		),
		BlipFB.ar(
			SinOsc.ar(0.019).range(-3000, 3000),
			LFPulse.kr(1.1).range(1, 512),
			LFNoise1.ar(3) * 20
		)
	])
});

options.numOutputBusChannels = 3;
options.numInputBusChannels = 0;
options.blockSize = 64;
options.memSize = 65536;

Routine({
	var file;
	var chunk = FloatArray.newClear(65536 * 3);
	var peak = 0.0;
	var sum = 0.0;
	var samples = 0;

	try {
		Score([
			[0, [\d_recv, definition.asBytes]],
			[0, [\s_new, \blipExtensionsLongRun, 1000, 0, 0]],
			[duration, [\n_free, 1000]]
		]).recordNRT(
			outputFilePath: path,
			sampleRate: 48000,
			headerFormat: "wav",
			sampleFormat: "float",
			options: options,
			duration: duration,
			action: { completed.value = true }
		);
		completed.value;
		file = SoundFile.openRead(path);
		while {
			file.readData(chunk);
			chunk.size > 0
		} {
			chunk.do { |value|
				if(value.isNaN or: { value.abs >= inf }) {
					Error("non-finite sample in long-run render").throw
				};
				peak = max(peak, value.abs);
				sum = sum + value;
				samples = samples + 1
			}
		};
		file.close;
		file = nil;
		File.delete(path);
		"BLIP_EXTENSIONS_LONG_RUN_OK peak=% mean=%"
		.format(peak, sum / samples).postln;
		0.exit
	} { |error|
		if(file.notNil) { file.close };
		if(File.exists(path)) { File.delete(path) };
		error.reportError;
		1.exit
	}
}).play(SystemClock);
)
