(
var names = [
	\ordinary, \triangle, \cosine, \peak, \decay, \fractional,
	\width, \dispersion, \attackDecay, \differentiated,
	\integrated, \power, \phaseModulated, \feedback
];
var functions = [
	{ BlipKernel.ar(90, 40, \rect, 1) },
	{ BlipKernel.ar(90, 40, \triangle, 1) },
	{ BlipKernel.ar(90, 40, \cosine, 1, 4) },
	{ BlipPeak.ar(90, 40, 0) },
	{ BlipDecay.ar(90, 40, 0.8) },
	{ BlipFrac.ar(90, SinOsc.kr(0.2).range(4, 40)) },
	{ BlipWidth.ar(90, 40, SinOsc.kr(0.2).range(0.03, 0.8)) },
	{ BlipDisp.ar(90, 40, SinOsc.kr(0.2) * 0.03) },
	{ BlipAD.ar(8, 80, 0.002, 0.08) },
	{ BlipDiff.ar(90, 40) },
	{ BlipInt.ar(90, 40) },
	{ BlipPower.ar(90, 40, 2) },
	{ BlipPM.ar(90, 40, SinOsc.ar(5) * 0.2) },
	{ BlipFB.ar(90, 40, 0.7) }
];

Routine({
	s.waitForBoot;
	names.do { |name, index|
		var synth;
		("AUDITION " ++ name).postln;
		synth = { Limiter.ar(functions[index].value * 0.12) ! 2 }.play;
		3.wait;
		synth.free;
		0.2.wait
	}
}).play(AppClock);
)
