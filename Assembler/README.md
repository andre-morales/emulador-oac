## :hammer: Assembler do Processador de OAC-I (EP3)

![](./exemplo.png)

Esse subprojeto implementa um editor em HTML5 embutido com um Assembler executável direto no navegador para a linguagem fictícia implementada pelo professor Nakano.
O programa permite a tradução do código direto para o formato .mem usado no Logisim e usado pelo emulador desenvolvido neste mesmo repositório.

## :toolbox: Recursos do Assembler/Editor:
- **Sintaxe colorizada**: Além de esteticamente agradável, auxilia a programação com cores descritivas das funcionalidades do código.
- **Labels**: Declaração e uso de labels para instruções e dados. Podendo atribuir nomes para variáveis e funções no código. Labels podem aparecer tanto antes quanto depois do seu uso.
- **Comentários**: Permitem organizar e transmitir a funcionalidade do código ao leitor.
- **Mnemônicos simplificados**: Pode-se utilizar labels diretamente nas instruções e realizar os cálculos com a instrução especial **calc**.
- **Ponteiro de escrita**: É possível a qualquer momento escolher em qual posição de memória as instruções e dados estão sendo emitidos. Os usos de labels se adaptarão automaticamente.
- **Mensagens de depuração**: O uso incorreto de algum recurso do assembly será apontado na linha onde o erro foi cometido, auxiliando o desenvolvimento.

## :arrow_forward: Uso
Não é necessário compilar nenhum código ou executar qualquer servidor. O aplicativo do jeito que é servido pode ser aberto diretamente no navegador ao executar o arquivo **index.html**. A interface é intuitiva e fácil de ser entendida sem manuais extras.
