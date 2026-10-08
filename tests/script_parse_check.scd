(
var root = thisProcess.nowExecutingPath.dirname.dirname;
var paths = [
	"prototypes/BlipExtensionsPrototypes.scd",
	"examples/audition.scd",
	"benchmarks/benchmark.scd",
	"tests/BlipExtensionsTests.scd",
	"tests/block_sample_rate_matrix.scd",
	"tests/compile_check.scd",
	"tests/long_run_stateful.scd",
	"tests/nonfinite_inputs.scd",
	"tests/prototype_check.scd",
	"tests/render_tests.scd",
	"tests/supernova_smoke.scd",
	"tests/verify_scdoc.scd"
];
var failed = false;

paths.do { |relativePath|
	var compiled = thisProcess.interpreter.compileFile(root +/+ relativePath);
	if(compiled.isNil) {
		("PARSE_FAILED " ++ relativePath).postln;
		failed = true
	} {
		("PARSED " ++ relativePath).postln
	}
};

if(failed) {
	"BLIP_EXTENSIONS_SCRIPT_PARSE_FAILED".postln;
	1.exit
} {
	"BLIP_EXTENSIONS_SCRIPT_PARSE_OK".postln;
	0.exit
};
)
