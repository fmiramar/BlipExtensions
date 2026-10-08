(
var sampleRates = [44100, 48000, 96000, 192000];
var blockSizes = [1, 16, 64, 128];
var channels = 13;
var duration = 0.04;

var definition = SynthDef(\blipExtensionsMatrix, {
	var freq = SinOsc.ar(17).range(-23000, 23000);
	var signals = [
		BlipSeries.ar(freq, 4096, SinOsc.ar(7).range(0, 4), SinOsc.ar(9).range(0, 3)),
		BlipKernel.ar(freq, 4096, \cosine, SinOsc.ar(3).range(0, 1), LFNoise1.kr(80).range(1, 8)),
		BlipPeak.ar(freq, 4096, SinOsc.ar(5).range(0, 1)),
		BlipDecay.ar(freq, 4096, SinOsc.ar(11).range(0, 1)),
		BlipFrac.ar(freq, LFTri.ar(43).range(1, 4096)),
		BlipWidth.ar(freq, 4096, SinOsc.ar(13).range(0, 1)),
		BlipDisp.ar(freq, 4096, LFNoise1.kr(90) * 100),
		BlipAD.ar(1000, 4096, 0, 0),
		BlipDiff.ar(freq, 4096),
		BlipInt.ar(freq, 4096),
		BlipPower.ar(freq, 4096, SinOsc.ar(19).range(0, 10), 1),
		BlipPM.ar(freq, 4096, SinOsc.ar(7000) * 1e8),
		BlipFB.ar(freq, 4096, SinOsc.ar(8000) * 1000)
	];
	Out.ar(0, signals)
});

Routine({
	try {
		sampleRates.do { |sampleRate|
			blockSizes.do { |blockSize|
				var path = Platform.defaultTempDir +/+
					"blip-extensions-matrix-%-%.wav"
					.format(sampleRate, blockSize);
				var options = ServerOptions.new;
				var completed = FlowVar.new;
				var soundFile;
				var data;

				options.numOutputBusChannels = channels;
				options.numInputBusChannels = 0;
				options.numAudioBusChannels = 256;
				options.blockSize = blockSize;
				options.memSize = 65536;

				Score([
					[0, [\d_recv, definition.asBytes]],
					[0, [\s_new, \blipExtensionsMatrix, 1000, 0, 0]],
					[duration, [\n_free, 1000]]
				]).recordNRT(
					outputFilePath: path,
					sampleRate: sampleRate,
					headerFormat: "wav",
					sampleFormat: "float",
					options: options,
					duration: duration,
					action: { completed.value = true }
				);
				completed.value;

				soundFile = SoundFile.openRead(path);
				data = FloatArray.newClear(
					soundFile.numFrames * soundFile.numChannels
				);
				soundFile.readData(data);
				soundFile.close;
				File.delete(path);

				if(data.any { |value|
					value.isNaN or: { value.abs >= inf }
				}) {
					Error(
						"non-finite matrix output at % Hz block %"
						.format(sampleRate, blockSize)
					).throw
				};
				"MATRIX_OK sampleRate=% blockSize=%"
				.format(sampleRate, blockSize).postln;
			}
		};
		"BLIP_EXTENSIONS_MATRIX_OK".postln;
		0.exit;
	} { |error|
		error.reportError;
		1.exit;
	}
}).play(SystemClock);
)
