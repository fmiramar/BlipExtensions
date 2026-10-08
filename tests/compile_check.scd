(
var krErrors = 0;
var rateErrors = 0;
var selectorErrors = 0;
var classes = [
	BlipSeries, BlipKernel, BlipPeak, BlipDecay, BlipFrac,
	BlipWidth, BlipDisp, BlipAD, BlipDiff, BlipInt,
	BlipPower, BlipPM, BlipFB
];

SynthDef(\blipExtensionsCompileAll, {
	var audioMul = SinOsc.ar(2).range(0.25, 0.75);
	var audioAdd = SinOsc.ar(3) * 0.01;
	var signals = [
		BlipSeries.ar(110, 20, 1, 2, audioMul, audioAdd),
		BlipKernel.ar(110, 20, \triangle, 1, 2, audioMul, audioAdd),
		BlipPeak.ar(110, 20, 1, audioMul, audioAdd),
		BlipDecay.ar(110, 20, SinOsc.ar(1).range(0, 1), audioMul, audioAdd),
		BlipFrac.ar(110, SinOsc.ar(1).range(1, 20), audioMul, audioAdd),
		BlipWidth.ar(110, 20, SinOsc.ar(1).range(0.05, 0.95), audioMul, audioAdd),
		BlipDisp.ar(110, 20, LFNoise1.kr(1) * 0.1, audioMul, audioAdd),
		BlipAD.ar(8, 20, 0.001, 0.05, audioMul, audioAdd),
		BlipDiff.ar(110, 20, audioMul, audioAdd),
		BlipInt.ar(110, 20, audioMul, audioAdd),
		BlipPower.ar(110, 20, SinOsc.ar(1).range(0.5, 3), \bipolar, audioMul, audioAdd),
		BlipPM.ar(110, 20, SinOsc.ar(5) * 0.2, audioMul, audioAdd),
		BlipFB.ar(110, 20, SinOsc.ar(1), audioMul, audioAdd)
	];
	Out.ar(0, Mix(signals) * 0)
}).asBytes;

classes.do { |class|
	try { class.kr } { krErrors = krErrors + 1 }
};

try {
	SynthDef(\badBlipKernelRate, {
		Out.ar(0, BlipKernel.ar(100, SinOsc.ar(1), \rect))
	}).asBytes
} {
	rateErrors = rateErrors + 1
};

try { BlipKernel.ar(100, 20, \unknown) } {
	selectorErrors = selectorErrors + 1
};
try { BlipPower.ar(100, 20, 2, \unknown) } {
	selectorErrors = selectorErrors + 1
};

if(krErrors != classes.size) {
	Error("Not every BlipExtensions class rejected .kr").throw
};
if(rateErrors != 1) {
	Error("Audio-rate discrete topology input was not rejected").throw
};
if(selectorErrors != 2) {
	Error("Symbol validation did not reject invalid selectors").throw
};

"BLIP_EXTENSIONS_COMPILE_OK".postln;
0.exit;
)
