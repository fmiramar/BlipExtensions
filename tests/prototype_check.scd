(
var prototypePath = thisProcess.nowExecutingPath.dirname.dirname
	+/+ "prototypes/BlipExtensionsPrototypes.scd";
var names = [
	\blipSeries, \blipKernel, \blipPeak, \blipDecay, \blipFrac,
	\blipWidth, \blipDisp, \blipAD, \blipDiff, \blipInt,
	\blipPower, \blipPM, \blipFB
];
var definitions;

try {
	thisProcess.interpreter.executeFile(prototypePath);
	names.do { |name|
		if(currentEnvironment[name].isNil) {
			Error("missing prototype: %".format(name)).throw
		}
	};
	definitions = [
		{ ~blipSeries.(110, 8, 1, 2) },
		{ ~blipKernel.(110, 8, \triangle, 0.75, 2) },
		{ ~blipPeak.(110, 8, 0.5) },
		{ ~blipDecay.(110, 8, 0.8) },
		{ ~blipFrac.(110, 8.5) },
		{ ~blipWidth.(110, 8, 0.2) },
		{ ~blipDisp.(110, 8, 0.01) },
		{ ~blipAD.(8, 8, 0.005, 0.08) },
		{ ~blipDiff.(110, 8) },
		{ ~blipInt.(110, 8) },
		{ ~blipPower.(110, 8, 2, 0) },
		{ ~blipPM.(110, 8, 0.2) },
		{ ~blipFB.(110, 8, 0.5) }
	];
	definitions.do { |function, index|
		SynthDef("blipPrototypeCheck%".format(index).asSymbol, {
			Out.ar(0, function.value * 0)
		}).asBytes
	};
	if(~blipFBOffline.(110, 8, 0.5, 128, 48000).any(_.isNaN)) {
		Error("offline BlipFB prototype emitted NaN").throw
	};
	"BLIP_EXTENSIONS_PROTOTYPES_OK".postln;
	0.exit
} { |error|
	error.reportError;
	1.exit
};
)
