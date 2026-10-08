(
var sampleRate = 48000;
var frames = 65536;
var duration = frames / sampleRate;
var channels = 14;
var outputDirectory = PathName(thisProcess.nowExecutingPath).pathOnly +/+ "renders";
var outputPath = outputDirectory +/+ "blip_extensions_spectral.wav";
var definition;
var options;
var completed = FlowVar.new;

File.mkdir(outputDirectory);

definition = SynthDef(\blipExtensionsSpectral, {
	var frequency = 375;
	var signals = [
		BlipSeries.ar(frequency, 20, 1, 1),
		BlipKernel.ar(frequency, 20, \rect, 1),
		BlipKernel.ar(frequency, 20, \triangle, 1),
		BlipKernel.ar(frequency, 20, \cosine, 1, 4),
		BlipPeak.ar(frequency, 20, 0),
		BlipDecay.ar(frequency, 20, 0.8),
		BlipFrac.ar(frequency, 12.5),
		BlipWidth.ar(frequency, 20, 0.25),
		BlipDisp.ar(frequency, 20, 0.013),
		BlipDiff.ar(frequency, 20),
		BlipInt.ar(frequency, 20),
		BlipPM.ar(frequency, 20, 0.2),
		BlipFB.ar(frequency, 20, 0),
		BlipPower.ar(frequency, 20, 2, \bipolar)
	];
	Out.ar(0, signals * 0.5)
});

options = ServerOptions.new;
options.numOutputBusChannels = channels;
options.numInputBusChannels = 0;
options.numAudioBusChannels = 256;
options.blockSize = 64;
options.memSize = 65536;

Routine({
	Score([
		[0, [\d_recv, definition.asBytes]],
		[0, [\s_new, \blipExtensionsSpectral, 1000, 0, 0]],
		[duration, [\n_free, 1000]]
	]).recordNRT(
		outputFilePath: outputPath,
		sampleRate: sampleRate,
		headerFormat: "wav",
		sampleFormat: "int16",
		options: options,
		duration: duration,
		action: {
			("BLIP_EXTENSIONS_RENDER_OK " ++ outputPath).postln;
			completed.value = true
		}
	);
	completed.value;
	0.exit
}).play(SystemClock);
)
