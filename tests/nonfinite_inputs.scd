(
var path = Platform.defaultTempDir +/+ "blip-extensions-nonfinite.wav";
var completed = FlowVar.new;
var channels = 13;
var options = ServerOptions.new;
var definition = SynthDef(\blipExtensionsNonfinite, {
	var nanAudio = DC.ar(0) / 0;
	var nanControl = DC.kr(0) / 0;
	var signals = [
		BlipSeries.ar(100, 20, nanAudio, 1),
		BlipKernel.ar(100, 20, \triangle, nanAudio),
		BlipPeak.ar(100, 20, nanAudio),
		BlipDecay.ar(100, 20, nanAudio),
		BlipFrac.ar(100, nanAudio),
		BlipWidth.ar(100, 20, nanAudio),
		BlipDisp.ar(100, 20, nanControl),
		BlipAD.ar(10, 20, nanControl, 0.1),
		BlipDiff.ar(DC.ar(inf), 20),
		BlipInt.ar(DC.ar(-inf), 20),
		BlipPower.ar(100, 20, nanAudio),
		BlipPM.ar(100, 20, nanAudio),
		BlipFB.ar(100, 20, DC.ar(inf))
	];
	Out.ar(0, signals)
});

options.numOutputBusChannels = channels;
options.numInputBusChannels = 0;
options.blockSize = 64;
options.memSize = 65536;

Routine({
	var file;
	var data;

	try {
		Score([
			[0, [\d_recv, definition.asBytes]],
			[0, [\s_new, \blipExtensionsNonfinite, 1000, 0, 0]],
			[0.02, [\n_free, 1000]]
		]).recordNRT(
			outputFilePath: path,
			sampleRate: 48000,
			headerFormat: "wav",
			sampleFormat: "float",
			options: options,
			duration: 0.02,
			action: { completed.value = true }
		);
		completed.value;
		file = SoundFile.openRead(path);
		data = FloatArray.newClear(file.numFrames * channels);
		file.readData(data);
		file.close;
		File.delete(path);

		if(data.any { |value|
			value.isNaN or: { value.abs >= inf }
		}) {
			Error("non-finite input escaped to output").throw
		};
		if(data.abs.maxItem > 1e-9) {
			Error("invalid parameter output was not deterministic zero").throw
		};
		"BLIP_EXTENSIONS_NONFINITE_OK".postln;
		0.exit
	} { |error|
		if(file.notNil) { file.close };
		if(File.exists(path)) { File.delete(path) };
		error.reportError;
		1.exit
	};
}).play(SystemClock);
)
