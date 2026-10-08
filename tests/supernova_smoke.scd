(
var server = Server(
	\blipExtensionsSupernova,
	NetAddr("127.0.0.1", 57133)
);

Server.supernova;

server.options.numOutputBusChannels = 2;
server.options.numInputBusChannels = 0;
server.options.numAudioBusChannels = 256;
server.options.blockSize = 64;
server.options.sampleRate = 48000;
server.options.memSize = 65536;

server.waitForBoot({
	Routine({
		var channels = 13;
		var frames = 4096;
		var buffer;
		var data;
		var condition;

		try {
			buffer = Buffer.alloc(server, frames, channels);
			server.sync;
			SynthDef(\blipExtensionsSupernovaSmoke, { |recordBuffer|
				var signals = [
					BlipSeries.ar(101, 32, 1, 2),
					BlipKernel.ar(103, 32, \cosine, 1, 4),
					BlipPeak.ar(107, 32, 1),
					BlipDecay.ar(109, 32, 0.8),
					BlipFrac.ar(113, 20.5),
					BlipWidth.ar(127, 32, 0.2),
					BlipDisp.ar(131, 32, 0.013),
					BlipAD.ar(17, 32, 0.001, 0.05),
					BlipDiff.ar(137, 32),
					BlipInt.ar(139, 32),
					BlipPower.ar(149, 32, 2),
					BlipPM.ar(151, 32, SinOsc.ar(7) * 0.1),
					BlipFB.ar(157, 32, 0.7)
				];
				RecordBuf.ar(
					signals, recordBuffer,
					loop: 0, doneAction: Done.freeSelf
				)
			}).add;
			server.sync;
			Synth(
				\blipExtensionsSupernovaSmoke,
				[\recordBuffer, buffer],
				server
			);
			0.15.wait;
			server.sync;

			condition = Condition({ data.notNil });
			buffer.loadToFloatArray(action: { |array|
				data = array;
				condition.signal
			});
			condition.wait;
			if(data.any { |value|
				value.isNaN or: { value.abs >= inf }
			}) {
				Error("supernova smoke render emitted NaN or infinity").throw
			};
			13.do { |channel|
				var peak = Array.fill(frames, { |frame|
					data[(frame * channels) + channel].abs
				}).maxItem;
				if(peak < 1e-6) {
					Error("supernova channel % stayed silent".format(channel)).throw
				}
			};

			"BLIP_EXTENSIONS_SUPERNOVA_OK".postln;
			buffer.free;
			server.sync;
			server.quit;
			Server.scsynth;
			0.exit;
		} { |error|
			error.reportError;
			if(buffer.notNil) { buffer.free };
			server.quit;
			Server.scsynth;
			1.exit;
		}
	}).play(SystemClock)
}, 15, {
	"BlipExtensions supernova server failed to boot".error;
	Server.scsynth;
	1.exit;
});

server.boot;
)
