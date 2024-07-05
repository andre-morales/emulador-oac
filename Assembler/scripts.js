"use strict"

var $text;
var $result;
var $info;
var $outputSelect
var editor;

function main() {
	$text = document.getElementById("text-area");
	$result = document.getElementById("assembly");
	$info = document.getElementById("info");
	$outputSelect = document.getElementById("output-fmt");
	editor = new Editor();
}

function copyContentToClipboard() {
	navigator.clipboard.writeText($result.value);
}

function saveContentAs() {
	let text = $result.value;
	
	// Create plain text blob content
	text = text.replace(/\n/g, "\r\n");
	let blob = new Blob([text], { type: "text/plain"});

	// Create hidden file download link
	let anchor = document.createElement("a");
	anchor.style.display = "none";
	anchor.download = getOutputFilename();
	anchor.href = window.URL.createObjectURL(blob);
	anchor.target = "_blank";

	// Add it to the DOM temporarily and click it
	document.body.appendChild(anchor);
	anchor.click();
	document.body.removeChild(anchor);
}

function getOutputFilename() {
	let outputFmt = $outputSelect.value;
	switch(outputFmt) {
		case 'logisim': return 'output.mem';
		case 'hex': return 'output.hex';
	}
	return 'output.txt';
}

function doAssemble() {
	$info.innerHTML = "";
	
	let compilation = new Compilation();

	let input = $text.value;
	try {
		if (compilation.perform(input)) {
			let fmt = $outputSelect.value;
			let output = compilation.output(fmt);
			$result.value = output;
		}
	} catch(err) {
		emitError(err);	
		throw err;
	}
}

function emitError(message) {
	$info.innerHTML += `<p>❗ ${message}</p>`;
}

function emitInfo(message) {
	$info.innerHTML += `<p>💬 ${message}</p>`;
}

class AssemblingError extends Error {
	constructor(message, lineNo) {
		super(message);
		this.name = "AssemblingError";		
		this.lineNo = lineNo;
	}
}

class AssemblySyntaxError extends AssemblingError {
	constructor(message) {
		super(message);
		this.name = "AssemblySyntaxError";
	}
}

class BadAddressError extends AssemblySyntaxError {
	constructor(message) {
		super(message);
		this.name = "BadAddressError";		
	}
}

function toHexStr(number) {
	let str = number.toString(16);
	let padding = 4 - str.length;
	for (let i = 0; i < padding; i++) {
		str = '0' + str;
	}
	return str;
}
