(
var server = Server(
	\blipExtensionsTests,
	NetAddr("127.0.0.1", 57131)
);

server.options.numOutputBusChannels = 2;
server.options.numInputBusChannels = 0;
server.options.numAudioBusChannels = 256;
server.options.blockSize = 64;
server.options.sampleRate = 48000;
server.options.memSize = 65536;

server.waitForBoot({
	Routine({
		var channels = 19;
		var frames = 8192;
		var buffer;
		var data;
		var condition;
		var channelData;
		var peaks;
		var maxDifference;

		try {
			buffer = Buffer.alloc(server, frames, channels);
			server.sync;

			SynthDef(\blipExtensionsRuntime, { |recordBuffer|
				var commonFreq = SinOsc.ar(0.7).range(-300, 300);
				var harmonicControl = LFPulse.kr(23).range(4, 48);
				var rawFB = BlipFB.ar(137, 24, 0.7);
				var scaledFB = BlipFB.ar(137, 24, 0.7, 2, 0.25);
				var rawPM = BlipPM.ar(173, 24, 0);
				var rawKernel = BlipKernel.ar(173, 24, \rect, 0);
				var signals = [
					BlipSeries.ar(commonFreq, harmonicControl, SinOsc.ar(3).range(0, 2), SinOsc.ar(5).range(0, 2)),
					BlipKernel.ar(commonFreq, harmonicControl, \triangle, SinOsc.ar(2).range(0, 1)),
					BlipPeak.ar(commonFreq, harmonicControl, SinOsc.ar(2).range(0, 1)),
					BlipDecay.ar(commonFreq, harmonicControl, SinOsc.ar(2).range(0, 1)),
					BlipFrac.ar(commonFreq, LFTri.ar(11).range(1, 64)),
					BlipWidth.ar(commonFreq, harmonicControl, SinOsc.ar(2.3).range(0.001, 0.999)),
					BlipDisp.ar(commonFreq, harmonicControl, LFNoise1.kr(30) * 0.1),
					BlipAD.ar(31, harmonicControl, LFNoise1.kr(20).range(0.0001, 0.01), LFNoise1.kr(20).range(0.02, 0.2)),
					BlipDiff.ar(commonFreq, harmonicControl),
					BlipInt.ar(commonFreq, harmonicControl),
					BlipPower.ar(commonFreq, harmonicControl, SinOsc.ar(3).range(0.2, 5), \bipolar),
					BlipPM.ar(commonFreq, harmonicControl, SinOsc.ar(701) * 0.3),
					BlipFB.ar(commonFreq, harmonicControl, SinOsc.ar(5) * 3),
					rawFB,
					scaledFB,
					rawPM,
					rawKernel,
					BlipDecay.ar(0, 32, 0.7),
					BlipWidth.ar(0, 32, SinOsc.ar(3).range(0.05, 0.95))
				];
				RecordBuf.ar(
					signals, recordBuffer,
					loop: 0, doneAction: Done.freeSelf
				)
			}).add;
			server.sync;

			Synth(\blipExtensionsRuntime, [\recordBuffer, buffer], server);
			0.25.wait;
			server.sync;

			condition = Condition({ data.notNil });
			buffer.loadToFloatArray(action: { |array|
				data = array;
				condition.signal
			});
			condition.wait;

			if(data.size != (frames * channels)) {
				Error("runtime test buffer size mismatch").throw
			};
			if(data.any { |value|
				value.isNaN or: { value.abs >= inf }
			}) {
				Error("a BlipExtensions signal emitted NaN or infinity").throw
			};

			channelData = { |channel|
				FloatArray.fill(frames, { |frame|
					data[(frame * channels) + channel]
				})
			};
			peaks = Array.fill(channels, { |channel|
				channelData.(channel).abs.maxItem
			});
			(0..18).do { |channel|
				if(peaks[channel] < 1e-6) {
					Error("runtime channel % stayed silent".format(channel)).throw
				}
			};

			maxDifference = Array.fill(frames, { |frame|
				(
					data[(frame * channels) + 14]
					- ((data[(frame * channels) + 13] * 2) + 0.25)
				).abs
			}).maxItem;
			if(maxDifference > 2e-5) {
				Error(
					"BlipFB mul/add entered feedback state (max error %)"
					.format(maxDifference)
				).throw
			};

			maxDifference = Array.fill(frames, { |frame|
				(
					data[(frame * channels) + 15]
					- data[(frame * channels) + 16]
				).abs
			}).maxItem;
			if(maxDifference > 2e-6) {
				Error(
					"BlipPM phase zero did not match rectangular kernel (max error %)"
					.format(maxDifference)
				).throw
			};

			"BLIP_EXTENSIONS_RUNTIME_OK peaks=%"
			.format(peaks.round(0.0001)).postln;

			buffer.free;
			server.sync;
			server.quit;
			0.exit;
		} { |error|
			error.reportError;
			if(buffer.notNil) { buffer.free };
			server.quit;
			1.exit;
		}
	}).play(SystemClock)
}, 15, {
	"BlipExtensions test server failed to boot".error;
	1.exit;
});

server.boot;
)
