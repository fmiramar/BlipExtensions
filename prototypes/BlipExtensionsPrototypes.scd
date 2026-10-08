/*
BlipExtensions mathematical reference prototypes

These graphs favor transparency over efficiency.  numharm/numPartials must be
plain integers because Array.fill and Mix.fill run while the SynthDef is built.
Native BlipExtensions UGens perform runtime harmonic limiting and transitions;
these references are intended for safe, static comparison cases.
*/

(
~blipPrototypePhase = { |freq|
	Phasor.ar(0, freq * SampleDur.ir * 2pi, -pi, pi, 0)
};

~blipSeries = { |freq = 440, numPartials = 20, first = 1, spacing = 1|
	var alpha = ~blipPrototypePhase.(freq * first);
	var beta = ~blipPrototypePhase.(freq * spacing);
	Mix.fill(numPartials.asInteger.max(1), { |index|
		(alpha + (index * beta)).cos
	}) / numPartials.asInteger.max(1)
};

~blipKernel = {
	|freq = 440, numharm = 20, kernel = \triangle, amount = 1, curve = 2|
	var phase = ~blipPrototypePhase.(freq);
	var n = numharm.asInteger.max(1);
	var weights = Array.fill(n, { |index|
		var target = switch(kernel,
			\rect, { 1 },
			\triangle, { 1 - (index / n) },
			\cosine, { cos(pi * index / (2 * n)).pow(curve) },
			{ Error("unknown BlipKernel prototype kernel").throw }
		);
		(1 - amount) + (amount * target)
	});
	Mix.fill(n, { |index| weights[index] * cos((index + 1) * phase) })
		/ weights.sum
};

~blipPeak = { |freq = 440, numharm = 20, center = 0|
	var phase = ~blipPrototypePhase.(freq);
	var n = numharm.asInteger.max(1);
	var peak = Mix.fill(n + 1, { |index|
		var k = index - n;
		(1 - (k.abs / (n + 1))) * cos(k * phase)
	}) / (n + 1);
	var dc = 1 / (n + 1);
	var centered = (peak - dc) / (1 - dc);
	peak.blend(centered, center.clip(0, 1))
};

~blipDecay = { |freq = 440, numharm = 20, decay = 0.9|
	var phase = ~blipPrototypePhase.(freq);
	var n = numharm.asInteger.max(1);
	var weights = Array.fill(n, { |index| decay.clip(0, 1).pow(index) });
	Mix.fill(n, { |index| weights[index] * cos((index + 1) * phase) })
		/ weights.sum
};

~blipFrac = { |freq = 440, numharm = 20|
	var phase = ~blipPrototypePhase.(freq);
	var h = numharm.max(1);
	var n = h.floor;
	var mu = h - n;
	(Mix.fill(n, { |index| cos((index + 1) * phase) })
		+ (mu * cos((n + 1) * phase))) / h
};

~blipWidth = { |freq = 440, numharm = 20, width = 0.1|
	var phase = ~blipPrototypePhase.(freq);
	var n = numharm.asInteger.max(1);
	var d = width.clip(0.00001, 0.99999);
	var bipolar = Mix.fill(n, { |index|
		var k = index + 1;
		(4 * sin(pi * k * d) / (pi * k)) * cos(k * phase)
	});
	bipolar / (2 * max(d, 1 - d))
};

~blipDisp = { |freq = 440, numharm = 20, dispersion = 0|
	var phase = ~blipPrototypePhase.(freq);
	var n = numharm.asInteger.clip(1, 2048);
	Mix.fill(n, { |index|
		var k = index + 1;
		cos((k * phase) + (dispersion * k.squared))
	}) / n
};

~blipAD = {
	|freq = 8, numharm = 80, attack = 0.005, decay = 0.08|
	var phase = ~blipPrototypePhase.(freq);
	var n = numharm.asInteger.max(1);
	var impulse = (1 + (2 * Mix.fill(n, { |index|
		cos((index + 1) * phase)
	}))) / (2 * n + 1);
	var aa = exp(-1 / (attack.max(SampleDur.ir) * SampleRate.ir));
	var ad = exp(-1 / (decay.max(attack + SampleDur.ir) * SampleRate.ir));
	var ya = OnePole.ar(impulse, aa);
	var yd = OnePole.ar(impulse, ad);
	yd - ya
};

~blipDiff = { |freq = 440, numharm = 20|
	var phase = ~blipPrototypePhase.(freq);
	var n = numharm.asInteger.max(1);
	0 - Mix.fill(n, { |index|
		var k = index + 1;
		k * sin(k * phase)
	}) / (n * (n + 1) * 0.5)
};

~blipInt = { |freq = 440, numharm = 20|
	var phase = ~blipPrototypePhase.(freq);
	var n = numharm.asInteger.clip(1, 2048);
	var norm = Array.series(n, 1, 1).reciprocal.sum;
	Mix.fill(n, { |index|
		var k = index + 1;
		sin(k * phase) / k
	}) / norm
};

~blipPower = {
	|freq = 440, numharm = 20, power = 2, polarity = 0|
	var input = ~blipKernel.(freq, numharm, \rect, 0, 2);
	var magnitude = input.abs.pow(power.max(0.001));
	Select.ar(polarity.clip(0, 1), [input.sign * magnitude, magnitude])
};

~blipPM = { |freq = 440, numharm = 20, phase = 0|
	var carrier = ~blipPrototypePhase.(freq) + phase;
	var n = numharm.asInteger.max(1);
	Mix.fill(n, { |index| cos((index + 1) * carrier) }) / n
};

// Exact only when the server block size is one.
~blipFB = { |freq = 440, numharm = 20, feedback = 0|
	var n = numharm.asInteger.max(1);
	var previous = LocalIn.ar(1, 0);
	var phase = ~blipPrototypePhase.(freq) + (feedback * previous / n);
	var raw = Mix.fill(n, { |index| cos((index + 1) * phase) }) / n;
	LocalOut.ar(raw);
	raw
};

// Offline scalar reference. Returns an Array of raw, pre-mul/add samples.
~blipFBOffline = {
	|freq = 440, numharm = 20, feedback = 0, numSamples = 1024,
	sampleRate = 48000|
	var n = numharm.asInteger.max(1);
	var phase = 0.0;
	var previous = 0.0;
	Array.fill(numSamples.asInteger.max(1), {
		var displaced = phase + (feedback * previous / n);
		previous = Array.fill(n, { |index|
			cos((index + 1) * displaced)
		}).sum / n;
		phase = (phase + (2pi * freq / sampleRate)).wrap(-pi, pi);
		previous
	})
};
)
