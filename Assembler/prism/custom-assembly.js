Prism.languages.custom_assembly = {
	'comment': /;.*/,
	'directive': {
		pattern: /\n\.[\w]+/,
		alias: 'property'
	},
	'data-directive': {
		pattern: /\b(?:dw|times)\b/i,
		alias: 'property'
	},
	'label': {
		pattern: /^[\s]*[^\s]+\:/m,
		alias: 'function'
	},
	'label-call': {
		pattern: /\ \:[\w.]+$/m,
		alias: 'selector'
	},
	'op-code': {
		pattern: /\b(?:NOP|LDA|STA|JMP|JNZ|ARIT|CALC|RET|HLT)\b/i,
		alias: 'keyword'
	},
	'hex-number': {
		pattern: /0x[\da-f]{1,4}\b/i,
		alias: 'number'
	},
	'binary-number': {
		pattern: /0b[01]+\b/,
		alias: 'number'
	},
	'decimal-number': {
		pattern: /\b\d+\b/,
		alias: 'number'
	},
	'register': {
		pattern: /\b([abcdr]|psw)\b/i,
		alias: 'variable'
	},
	'punctuation': /[(),:]/

	/*'string': /(["'`])(?:\\.|(?!\1)[^\\\r\n])*\1/,*/
}