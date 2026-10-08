(
var pages = [
	"Classes/BlipSeries",
	"Classes/BlipKernel",
	"Classes/BlipPeak",
	"Classes/BlipDecay",
	"Classes/BlipFrac",
	"Classes/BlipWidth",
	"Classes/BlipDisp",
	"Classes/BlipAD",
	"Classes/BlipDiff",
	"Classes/BlipInt",
	"Classes/BlipPower",
	"Classes/BlipPM",
	"Classes/BlipFB",
	"Guides/BlipExtensions"
];
var ok = true;

SCDoc.verbosity = 2;
SCDoc.indexAllDocuments(true);

pages.do { |path|
	var document = SCDoc.documents[path];
	if(document.isNil) {
		("MISSING_DOC " ++ path).postln;
		ok = false
	} {
		var root = SCDoc.parseDoc(document);
		if(root.isNil) {
			("PARSE_FAILED " ++ path).postln;
			ok = false
		} {
			SCDoc.renderer.renderToFile(
				document.destPath, document, root
			);
			("RENDERED " ++ path).postln
		}
	}
};

if(ok) {
	"BLIP_EXTENSIONS_SCDOC_OK".postln;
	0.exit
} {
	"BLIP_EXTENSIONS_SCDOC_FAILED".postln;
	1.exit
};
)
