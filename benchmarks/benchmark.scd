(
/*
Real-time server benchmark.

Edit these two labels if using a different binary, then evaluate the file.
Cases print absolute average/peak server CPU, not percentages relative to
another UGen. Results vary with hardware and audio-driver load.
*/
var compiler = "AppleClang 17";
var buildType = "Release";
var server = Server(
	\blipExtensionsBenchmark,
	NetAddr("127.0.0.1", 57132)
);
var harmonicCounts = [8, 32, 128, 512, 2048];
var instanceCounts = [1, 100, 1000];
var cases;

server.options.numOutputBusChannels = 2;
server.options.numInputBusChannels = 0;
server.options.numAudioBusChannels = 4096;
server.options.numWireBufs = 8192;
server.options.blockSize = 64;
server.options.sampleRate = 48000;
server.options.memSize = 262144;
server.options.maxNodes = 8192;

cases = [
	[\Blip, { |n| Blip.ar(80, n) }],
	[\BlipSeries, { |n| BlipSeries.ar(80, n, 1, 1) }],
	[\BlipKernelRect, { |n| BlipKernel.ar(80, n, \rect, 1) }],
	[\BlipKernelCos4, { |n| BlipKernel.ar(80, n, \cosine, 1, 4) }],
	[\BlipPeak, { |n| BlipPeak.ar(80, n) }],
	[\BlipDecayStatic, { |n| BlipDecay.ar(80, n, 0.8) }],
	[\BlipDecayModulated, { |n| BlipDecay.ar(80, n, SinOsc.ar(3).range(0, 1)) }],
	[\BlipFracModulated, { |n| BlipFrac.ar(80, SinOsc.ar(3).range(1, n)) }],
	[\BlipWidth, { |n| BlipWidth.ar(80, n, 0.25) }],
	[\Pulse, { Pulse.ar(80, 0.25) }],
	[\BlipAD, { |n| BlipAD.ar(80, n, 0.001, 0.05) }],
	[\BlipDiff, { |n| BlipDiff.ar(80, n) }],
	[\BlipPM, { |n| BlipPM.ar(80, n, SinOsc.ar(31) * 0.1) }],
	[\BlipFB, { |n| BlipFB.ar(80, n, 0.7) }],
	[\BlipDisp, { |n| BlipDisp.ar(80, n, 0.013) }],
	[\BlipInt, { |n| BlipInt.ar(80, n) }],
	[\Klang, { |n|
		var count = n.min(512);
		Klang.ar(Ref([
			Array.series(count, 80, 80),
			Array.fill(count, 1 / count),
			nil
		]))
	}],
	[\MixFill, { |n|
		Mix.fill(n.min(512), { |index|
			SinOsc.ar(80 * (index + 1), 0, 1 / n.min(512))
		})
	}]
];

server.waitForBoot({
	Routine({
		"BLIP_EXTENSIONS_BENCHMARK".postln;
		"server=% sampleRate=% blockSize=% os=% arch=% compiler=% build=%"
		.format(
			Server.program, server.sampleRate, server.options.blockSize,
			Platform.name, Platform.architecture, compiler, buildType
		).postln;

		cases.do { |entry|
			harmonicCounts.do { |harmonics|
				instanceCounts.do { |instances|
					var definitionName = (
						"blipBench_" ++ entry[0] ++ "_" ++ harmonics
					).asSymbol;
					var group;
					if(
						([\MixFill, \Klang].includes(entry[0]))
						and: { (harmonics > 512) or: { instances > 100 } }
					) {
						"SKIP class=% harmonics=% instances=% reason=graph-size"
						.format(entry[0], harmonics, instances).postln
					} {
						SynthDef(definitionName, {
							Out.ar(0, entry[1].value(harmonics) * 0)
						}).add;
						server.sync;
						group = Group(server);
						instances.do {
							Synth(definitionName, target: group)
						};
						2.wait;
						"RESULT class=% harmonics=% instances=% avgCPU=% peakCPU=%"
						.format(
							entry[0], harmonics, instances,
							server.avgCPU.round(0.001),
							server.peakCPU.round(0.001)
						).postln;
						group.free;
						server.sync;
						0.5.wait
					}
				}
			}
		};
		"BLIP_EXTENSIONS_BENCHMARK_DONE".postln;
		server.quit;
	}).play(SystemClock)
}, 15, {
	"benchmark server failed to boot".error
});

server.boot;
)
