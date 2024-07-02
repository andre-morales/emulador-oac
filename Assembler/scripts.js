"use strict"

var $text;
var $result;
var $info;

function main() {
	$text = document.getElementById("text-area");
	$result = document.getElementById("assembly");
	$info = document.getElementById("info");
}

function doAssemble() {
	let compilation = new Compilation();
	if (compilation.perform()) {
		let output = compilation.output();
		$result.value = output;
	}
}

function emitHex(number, count) {
	if (count === undefined) count = 1;

	let str = number.toString(16);
	let padding = 4 - str.length;
	for (let i = 0; i < padding; i++) {
		str = '0' + str;
	}
	
	if (count != 1) {
		resultString += count + "*";
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
	$info.innerHTML += `<p>❗ ${message}</p>`;
}

function emitInfo(message) {
	$info.innerHTML += `<p>💬 ${message}</p>`;
}

function toHexStr(number) {
	let str = number.toString(16);
	let padding = 4 - str.length;
	for (let i = 0; i < padding; i++) {
		str = '0' + str;
	}
	return str;
}

function toAddress(str) {
	let address = Number(str)
	if (address < 0) throw new BadAddressSyntax();
	if (address > 0xFFF) throw new BadAddressSyntax();
	return address;
}	

class Compilation {
	constructor() {
		this.lineNo = 1;
		this.position = 0;
		this.fillPattern = 0x0000;
		this.memory = [];
		this.labels = {};
		this.fixups = new Map();
	}

	perform() {
		$info.innerHTML = "";

		let content = $text.value;
		let lines = content.split("\n");
		let compilationError;

		try {
			for (; this.lineNo < lines.length; this.lineNo++) {
				let line = lines[this.lineNo - 1].trim().toLowerCase();
				if (!line) continue;

				// If line is a label definition
				if (line.endsWith(":")) {
					let labelName = line.substring(0, line.length - 1);
					let position = this.position;
					this.labels[labelName] = position;

					// Fix all fixups associated with this label, if any;
					let fixes = this.fixups.get(labelName);
					if (fixes) {
						console.log(fixes);
						for (let fix of fixes) {
							console.log('fixing' + fix);
							this.memory[fix] |= position;
						}
						this.fixups.delete(labelName);
					}
					continue;
				}

				let tokens = line.split(" ");
				let instr = tokens[0];
				switch(instr) {
				case 'dw':
					this.interpretDataDirective(line);				
					break;
				case 'times':
					let times = Number(tokens[1]);
					for (let i = 0; i < times; i++) {
						this.interpretDataDirective(tokens.slice(2).join(' '));
					}
					break;
				case 'nop':
					this.emit(0x0000);
					break;
				case 'lda': {
					let address = this.getTarget(tokens[1]);
					this.emit(0x1000 | address);
					break;
				}
				case 'sta': {
					let address = this.getTarget(tokens[1]);
					this.emit(0x2000 | address);
					break;
				}
				case 'jmp': {
					let address = this.getTarget(tokens[1]);
					this.emit(0x3000 | address);
					break;
				}
				case 'jnz': {
					let address = this.getTarget(tokens[1]);
					this.emit(0x4000 | address);
					break;
				}
				case 'arit': {
					let opcode = this.interpretArit(line);
					this.emit(opcode);
					break;
				}
				case 'calc': {
					let opcode = this.interpretCalc(line);
					this.emit(opcode);
					break;
				}
				case 'hlt':
					this.emit(0xFFFF);
					break;
				default:
					throw new BadSyntax(`Unknown mnemonic '${tokens[0]}'`);
				}
			}

			if (this.fixups.size) {
				throw new AssemblingError("Compilation finished with incomplete fixups!");
			}
			
		} catch(err) {
			if (!(err instanceof AssemblingError)) {
				throw err;
			}
			compilationError = err;
			emitError("Compilation terminated! Line " + this.lineNo + ": " + err);
		}
		
		if (compilationError) {
			emitError("Assembling errored out.");
			return false;
		}

		emitInfo("Assembling successful.");
		return true;
	}
	
	interpretDataDirective(line) {
		let tokens = line.split(" ");
		if (tokens[0] == 'dw') {
			let number = Number(tokens[1]);
			if (number > 0xFFFF) throw new BadSyntax("Number exceeds word size.");
			this.emit(number);
		} else {
			throw BadSyntax("Bad data directive: " + tokens[0]);
		}
	}
	
	interpretArit(line) {
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
	
	interpretCalc(line) {
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
			default:
				throw new BadSyntax("Invalid CALC operation '" + args[3] + "'");
			}
			reg2 = args[4];
		}
	
		let arit = `arit ${operation}, ${destination}, ${reg1}, ${reg2}`;
		return this.interpretArit(arit);
	}

	getTarget(target) {
		// If target is a label
		if (target.startsWith(":")) {
			// Try acquiring the position of the label and return it if it's available.
			let labelName = target.substring(1);
			let position = this.labels[labelName];
			if (position !== undefined) return position;

			// Label was not defined yet, save it as a fixup and return 0.
			// Get fixup list for this label, or create one if necessary.
			let fixList = this.fixups.get(labelName);
			if (!fixList) {
				fixList = [];
				this.fixups.set(labelName, fixList);
			}

			// Save the current memory position as a pending fixup for the given label.
			fixList.push(this.position);
			return 0x0;
		}

		// Target is a pure address, convert it and return
		let address = toAddress(target);
		return address;
	}

	output() {
		const bundling = false;
		let output = "";

		for (let i = 0; i < this.memory.length; ) {
			let times = 1;
			let value = this.memory[i];

			if (bundling) {
				while(value == this.memory[++i]) {
					times++;
				}

				if (times != 1) {
					output += times + "*";
				}
			} else {
				i++;
			}

			output += toHexStr(value) + ' ';
		}
		return output;
	}

	moveHeadTo(location) {
		if(location < this.position) {
			throw new Error("File pointer writer cannot move backwards!");
		}
		this.position = location;
	}

	advanceHead(increment) {
		if (increment < 0) {
			throw new Error("File pointer writer cannot move backwards!");
		}
		this.position += increment;
	}

	emit(hex, times) {
		if (times === undefined) times = 1;
		
		// Save current memory write pointer.
		let currentPointer = this.memory.length;

		// Fill from pointer onwards with the hex value desired
		this.memory[this.position + times - 1] = hex;
		this.memory.fill(hex, this.position, this.position + times);

		// If the write head was moved, fill memory array with fill pattern
		this.memory.fill(this.fillPattern, currentPointer, this.position);

		this.position += times;
	}
}

class AssemblingError extends Error {
	constructor(message) {
		super(message);
		this.name = "AssemblingError";		
	}
}

class BadSyntax extends AssemblingError {
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

