// Versao web: argumentos da linha de comando pela URL, separados por &.
//   index.html?--direto&--qualidade&baixo&--sem-entrada&--roteiro-circuito&6&--congelar-no-quadro&60
// Serve para repetir no navegador os mesmos roteiros e capturas do build nativo.
if (typeof location !== 'undefined' && location.search.length > 1) {
  Module['arguments'] = location.search.substring(1).split('&').filter(function (a) { return a.length > 0; }).map(decodeURIComponent);
}
