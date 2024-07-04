class Editor {
	constructor() {
		// Text area internal padding. This is OS-dependent. Increasing it will make the editor
		// think the area is smaller.
		this.PADDING_ADJUST = 5;

		this.tabWidth = 4;
		this.$text = document.getElementById("text-area");
		this.$lineNumbers = document.getElementById("line-numbers");

		this.tabReplacement = new Array(this.tabWidth + 1).join(' ');
		this.makeGraphicsContext();

		// Mirror text area scrolling
		this.$text.addEventListener('scroll', () => {
			this.$lineNumbers.scrollTop = this.$text.scrollTop;
		});

		// Update line numbers if necessary
		this.$text.addEventListener('input', () => {
			this.displayLineNumbers();
		});

		// Update line numbers on resizes
		const ro = new ResizeObserver(() => {
			this.displayLineNumbers();
		});
		ro.observe(this.$text);
	}

	makeGraphicsContext() {
		// Discover font used in text area
		let styles = window.getComputedStyle(this.$text);

		// Create graphics context trough canvas and set the same font
		let canvas = document.createElement('canvas');
		this.grContext = canvas.getContext('2d');
		//this.grContext.font = styles.font;
		this.grContext.font = `${styles.fontSize} ${styles.fontFamily}`
	}

	calculateNumLines(sentence) {
		// We must replace all tabs in the sentence. Canvas does not take into account tab sizes
		// properly.
		sentence = sentence.replaceAll('\t', this.tabReplacement);
		const parseValue = (v) => v.endsWith('px') ? parseInt(v.slice(0, -2), 10) : 0;

		// Get all CSS properties in the textarea
		const fieldStyles = window.getComputedStyle(this.$text);

		// Discover the true size of the text area
		let paddingLeft = parseValue(fieldStyles.paddingLeft);
		let paddingRight = parseValue(fieldStyles.paddingRight);
		let fieldWidth = this.$text.getBoundingClientRect().width - paddingLeft - paddingRight - this.PADDING_ADJUST;

		let lineCount = 0;
		let currentLine = '';
		let words = sentence.split(' ');
		for (let word of words) {
			let wordWidth = this.grContext.measureText(word + ' ').width;
			let lineWidth = this.grContext.measureText(currentLine).width;
			let sumWidth = lineWidth + wordWidth;
			//let sumWidth = this.grContext.measureText(currentLine + word + ' ').width;
			if (sumWidth > fieldWidth) {
				lineCount++;
				currentLine = word + ' ';
			} else {
				currentLine += word + ' ';
			}
		}

		if (currentLine.trim() !== '') {
			lineCount++;
		}

		if (lineCount > 1) {
			console.log("wrapped: " + sentence);
			for(let word of words) {
				console.log("w: " + word, "h: " + this.grContext.measureText(word + ' ').width);
			}
		}
		return lineCount;
	}

	makeLineNumbers() {
		// Calculate how many lines there are in each true line of the content.
		let textLines = this.$text.value.split('\n');
		let lineCounts = textLines.map((line) => this.calculateNumLines(line));
		
		let numbers = [];

		// For every line count, we'll populate numbers with the correct index or empty padding
		// holes.
		for (let i = 0; i < lineCounts.length; i++) {
			numbers.push(i + 1);
			
			// Create empty holes as necessary
			let num = lineCounts[i];
			for (let j = 1; j < num; j++) {
				numbers.push('');
			}
		}
	
		return numbers;
	}

	displayLineNumbers() {
		// For each line number in the array, make a div element
		let numbers = this.makeLineNumbers();
		let content = numbers.map(n => `<div>${n || '&nbsp;'}</div>`).join('');
		this.$lineNumbers.innerHTML = content;
	}
}