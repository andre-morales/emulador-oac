"use strict"

var $text;
var $result;
var $info;
var resultString;

function main() {
	$text = document.getElementById("text-area");
	$result = document.getElementById("assembly");
	$info = document.getElementById("info");
}

function doAssemble() {
	resultString = "";
	$info.innerHTML = "";

	let content = $text.value;
	let lines = content.split("\n");
	let lineNo = 0;
	let compilationError;

	try {
		for (; lineNo < lines.length; lineNo++) {
			let line = lines[lineNo].trim().toLowerCase();
			if (!line) continue;

			let tokens = line.split(" ");
			let instr = tokens[0];
			switch(instr) {
			case 'nop':
				emitHex(0x0000);
				break;
			case 'lda': {
				let address = toAddress(tokens[1]);
				emitHex(0x1000 | address);
				break;
			}
			case 'sta': {
				let address = toAddress(tokens[1]);
				emitHex(0x2000 | address);
				break;
			}
			case 'jmp': {
				let address = toAddress(tokens[1]);
				emitHex(0x3000 | address);
				break;
			}
			case 'jnz': {
				let address = toAddress(tokens[1]);
				emitHex(0x4000 | address);
				break;
			}
			case 'arit': {
				let opcode = interpretArit(line);
				emitHex(opcode);
				break;
			}
			case 'calc': {
				let opcode = interpretCalc(line);
				emitHex(opcode);
				break;
			}
			case 'hlt':
				emitHex(0xFFFF);
				break;
			default:
				throw new BadSyntax(`Unknown mnemonic '${tokens[0]}'`);
			}
		}
	} catch(err) {
		compilationError = err;
		emitError("Compilation terminated! " + err);
	}
	
	if (compilationError) {
		emitError("Assembling errored out.");
	} else {
		$result.value = resultString;
		emitInfo("Assembling successful.");
	}
}

function interpretArit(line) {
	let argsStr = line.substring("arit".length).trim();
	let args = argsStr.split(',');
	let opcode = 0x6000;

	let operation = args[0].trim();
	switch(operation) {
	case '0':   opcode |= 0b000000000000; break;
	case 'f':   opcode |= 0b001000000000; break;
	case 'not': opcode |= 0b010000000000; break;
	case 'and': opcode |= 0b011000000000; break;
	case 'or':  opcode |= 0b100000000000; break;
	case 'xor': opcode |= 0b101000000000; break;
	case 'add': opcode |= 0b110000000000; break;
	case 'sub': opcode |= 0b111000000000; break;
	default:
		throw new BadSyntax(`Invalid arit operation '${operation}'`);
	}

	let destination = args[1].trim();
	opcode |= (getRegisterCode(destination) << 6);

	let reg1 = args[2].trim();
	opcode |= (getRegisterCode(reg1) << 3);

	let reg2 = args[3].trim();
	switch(reg2) {
		case '0':
		case 'zero':
					opcode |= 0b000; break;
		case 'a':   opcode |= 0b100; break;
		case 'b':   opcode |= 0b101; break;
		case 'c':   opcode |= 0b110; break;
		case 'd':   opcode |= 0b111; break;
		default:
			throw new BadSyntax(`Invalid arit second register '${reg1}'`);
	}

	return opcode;
}

function interpretCalc(line) {
	let argsStr = line.substring("calc".length).trim();
	let args = argsStr.split(' ');
	
	let destination = args[0];
	let operation = '';
	let reg1 = 'a';
	let reg2 = '0';

	if (args[1] != '=') {
		throw BadSyntax("Calc instruction missing equals sign!");
	}

	// Short CALC instructions of the form CALC D = XXX
	if (args.length == 3) {
		// Case for CALC D = ~R. Emit it as ARIT NOT D, R, 0
		if (args[2].startsWith('~')) {
			operation = 'not';
			reg1 = args[2].substring(1);
		// Case for CALC D = 0
		} else if(args[2].startsWith('0')) {
			operation = '0';
		// Case for CALC D = F
		} else if(args[2].startsWith('f')) {
			operation = 'f';
		// Case for CALC D = R. Emit is as ARIT ADD, D, R, 0
		} else {
			reg1 = args[2];
			operation = 'add';
		}
	// General CALC format as CALC D = R @ S
	} else {
		reg1 = args[2];
		switch(args[3]) {
		case '&': operation = 'and'; break;
		case '|': operation = 'or'; break;
		case '^': operation = 'xor'; break;
		case '+': operation = 'add'; break;
		case '-': operation = 'sub'; break;
		}
		reg2 = args[4];
	}

	let arit = `arit ${operation}, ${destination}, ${reg1}, ${reg2}`;
	return interpretArit(arit);
}

function toAddress(str) {
	let address = Number(str)
	if (address < 0) throw new BadAddressSyntax();
	if (address > 0xFFF) throw new BadAddressSyntax();
	return address;
}

function emitHex(number) {
	let str = number.toString(16);
	let padding = 4 - str.length;
	for (let i = 0; i < padding; i++) {
		str = '0' + str;
	}
	resultString += str + " ";
}

function getRegisterCode(register, lower) {
	if (lower && (register == 'psw' || register == 'r')) {
		throw new BadSyntax(`The register '${register}' cannot be addressed here`);
	}
	switch(register) {
		case 'a':   return 0b000;
		case 'b':   return 0b001;
		case 'c':   return 0b010;
		case 'd':   return 0b011;
		case 'r':   return 0b110;
		case 'psw': return 0b111;
		default:
			throw new BadSyntax(`Invalid register '${register}'`);
	}
}

function emitError(message) {
	$info.innerHTML += `<p>[E] ${message}</p>`;
}

function emitInfo(message) {
	$info.innerHTML += `<p>[i] ${message}</p>`;
}

class BadSyntax extends Error {
	constructor(message) {
		super(message);
		this.name = "BadSyntax";
	}
}

class BadAddressSyntax extends BadSyntax {
	constructor(message) {
		super(message);
		this.name = "BadAddressSyntax";		
	}
}