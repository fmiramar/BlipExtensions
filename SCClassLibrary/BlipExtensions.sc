// SPDX-License-Identifier: GPL-3.0-or-later

BlipSeries : UGen {
	*ar {
		|freq = 440, numPartials = 20, first = 1, spacing = 1, mul = 1, add = 0|
		^this.multiNew('audio', freq, numPartials, first, spacing).madd(mul, add)
	}

	checkInputs {
		if(inputs[1].rate == \audio) {
			^"BlipSeries numPartials must be scalar or control rate"
		};
		^this.checkValidInputs
	}
}

BlipKernel : UGen {
	*kernelIndex { |kernel|
		if(kernel.isNumber) { ^kernel.asInteger.clip(0, 2) };
		^#[rect, triangle, cosine].indexOfEqual(kernel)
			?? { Error("BlipKernel kernel must be \\rect, \\triangle, or \\cosine").throw }
	}

	*ar {
		|freq = 440, numharm = 20, kernel = \triangle, amount = 1, curve = 2,
		mul = 1, add = 0|
		^this.multiNew(
			'audio', freq, numharm, this.kernelIndex(kernel), amount, curve
		).madd(mul, add)
	}

	checkInputs {
		if(inputs[1].rate == \audio) {
			^"BlipKernel numharm must be scalar or control rate"
		};
		if(inputs[2].rate != \scalar) {
			^"BlipKernel kernel must be initialization rate"
		};
		if(inputs[4].rate == \audio) {
			^"BlipKernel curve must be scalar or control rate"
		};
		^this.checkValidInputs
	}
}

BlipPeak : UGen {
	*ar { |freq = 440, numharm = 20, center = 0, mul = 1, add = 0|
		^this.multiNew('audio', freq, numharm, center).madd(mul, add)
	}

	checkInputs {
		if(inputs[1].rate == \audio) {
			^"BlipPeak numharm must be scalar or control rate"
		};
		^this.checkValidInputs
	}
}

BlipDecay : UGen {
	*ar { |freq = 440, numharm = 20, decay = 0.9, mul = 1, add = 0|
		^this.multiNew('audio', freq, numharm, decay).madd(mul, add)
	}

	checkInputs {
		if(inputs[1].rate == \audio) {
			^"BlipDecay numharm must be scalar or control rate"
		};
		^this.checkValidInputs
	}
}

BlipFrac : UGen {
	*ar { |freq = 440, numharm = 20, mul = 1, add = 0|
		^this.multiNew('audio', freq, numharm).madd(mul, add)
	}
}

BlipWidth : UGen {
	*ar { |freq = 440, numharm = 20, width = 0.1, mul = 1, add = 0|
		^this.multiNew('audio', freq, numharm, width).madd(mul, add)
	}

	checkInputs {
		if(inputs[1].rate == \audio) {
			^"BlipWidth numharm must be scalar or control rate"
		};
		^this.checkValidInputs
	}
}

BlipDisp : UGen {
	*ar { |freq = 440, numharm = 20, dispersion = 0, mul = 1, add = 0|
		^this.multiNew('audio', freq, numharm, dispersion).madd(mul, add)
	}

	checkInputs {
		if(inputs[1].rate == \audio) {
			^"BlipDisp numharm must be scalar or control rate"
		};
		if(inputs[2].rate == \audio) {
			^"BlipDisp dispersion must be scalar or control rate"
		};
		^this.checkValidInputs
	}
}

BlipAD : UGen {
	*ar {
		|freq = 8, numharm = 80, attack = 0.005, decay = 0.08, mul = 1, add = 0|
		^this.multiNew('audio', freq, numharm, attack, decay).madd(mul, add)
	}

	checkInputs {
		if(inputs[1].rate == \audio) {
			^"BlipAD numharm must be scalar or control rate"
		};
		if(inputs[2].rate == \audio or: { inputs[3].rate == \audio }) {
			^"BlipAD attack and decay must be scalar or control rate"
		};
		^this.checkValidInputs
	}
}

BlipDiff : UGen {
	*ar { |freq = 440, numharm = 20, mul = 1, add = 0|
		^this.multiNew('audio', freq, numharm).madd(mul, add)
	}

	checkInputs {
		if(inputs[1].rate == \audio) {
			^"BlipDiff numharm must be scalar or control rate"
		};
		^this.checkValidInputs
	}
}

BlipInt : UGen {
	*ar { |freq = 440, numharm = 20, mul = 1, add = 0|
		^this.multiNew('audio', freq, numharm).madd(mul, add)
	}

	checkInputs {
		if(inputs[1].rate == \audio) {
			^"BlipInt numharm must be scalar or control rate"
		};
		^this.checkValidInputs
	}
}

BlipPower {
	*polarityIndex { |polarity|
		if(polarity.isNumber) { ^polarity.asInteger.clip(0, 1) };
		^#[bipolar, unipolar].indexOfEqual(polarity)
			?? { Error("BlipPower polarity must be \\bipolar or \\unipolar").throw }
	}

	*ar {
		|freq = 440, numharm = 20, power = 2, polarity = 0, mul = 1, add = 0|
		var input = BlipKernel.ar(freq, numharm, \rect, 0);
		var magnitude;
		var output;
		var valid = 1;
		power = if(power.isNumber) {
			if(power.isNaN or: { power.abs >= inf }) {
				valid = 0;
				1
			} {
				power.max(0.001)
			}
		} {
			var cleanPower = power.sanitize;
			// A finite value equals its sanitized form. For NaN, infinity, or a
			// subnormal, the subtraction is non-finite/subnormal and the outer
			// Sanitize replaces it with one, producing a zero validity mask.
			valid = 1 - Sanitize.ar((power - cleanPower).abs, 1).clip(0, 1);
			cleanPower.max(0.001)
		};
		magnitude = input.abs.pow(power);
		polarity = this.polarityIndex(polarity);
		output = if(polarity == 0) { input.sign * magnitude } { magnitude };
		^(output * valid).madd(mul, add)
	}
}

BlipPM : UGen {
	*ar { |freq = 440, numharm = 20, phase = 0, mul = 1, add = 0|
		^this.multiNew('audio', freq, numharm, phase).madd(mul, add)
	}

	checkInputs {
		if(inputs[1].rate == \audio) {
			^"BlipPM numharm must be scalar or control rate"
		};
		^this.checkValidInputs
	}
}

BlipFB : UGen {
	*ar { |freq = 440, numharm = 20, feedback = 0, mul = 1, add = 0|
		^this.multiNew('audio', freq, numharm, feedback).madd(mul, add)
	}

	checkInputs {
		if(inputs[1].rate == \audio) {
			^"BlipFB numharm must be scalar or control rate"
		};
		^this.checkValidInputs
	}
}
