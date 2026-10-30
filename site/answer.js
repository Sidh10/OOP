// Turns a typed phrase ("momentum on btc") into the engine's real answer from reports/latest_recommendation.csv.
// Plain script, no modules, so it runs both in the browser and in build.mjs (which pre-renders the default answer).
(function (root) {
  var STRATS = [
    { key: 'Breakout', label: 'Breakout', re: /break/ },
    { key: 'Momentum', label: 'Momentum', re: /moment|trend/ },
    { key: 'MeanReversion', label: 'Mean reversion', re: /mean|revers/ },
    { key: 'MLSignal', label: 'ML signal', re: /\bml\b|machine|logistic|learn|signal/ },
    { key: 'PairsTrading', label: 'Pairs trading', re: /pair|spread|coint/ }
  ];
  var COIN_RE = /\b(btc|bitcoin|eth|ethereum|bnb|sol|solana)(usdt)?\b/g;
  var COIN_NAME = { bitcoin: 'BTC', btc: 'BTC', ethereum: 'ETH', eth: 'ETH', bnb: 'BNB', sol: 'SOL', solana: 'SOL' };
  var VERDICT = { REJECT: 'Rejected', WEAK: 'Weak', RECOMMENDED: 'Recommended', INSUFFICIENT: 'Too few trades' };

  function esc(s) {
    return String(s).replace(/[&<>"]/g, function (c) { return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]; });
  }
  function num(x, d) {
    var s = Math.abs(x).toFixed(d == null ? 2 : d);
    return (x < 0 && Number(s) !== 0 ? '−' : '') + s;
  }
  function pct(x, d) { return num(x * 100, d == null ? 1 : d) + '%'; }
  function coinOf(universe) { return universe.replace(/USDT/g, ''); }
  function stratLabel(key) { for (var i = 0; i < STRATS.length; i++) if (STRATS[i].key === key) return STRATS[i].label; return key; }

  function parse(text, rows) {
    var t = String(text).toLowerCase();
    var strat = null, at = Infinity;
    for (var i = 0; i < STRATS.length; i++) {            // the strategy named first in the text wins
      var pos = t.search(STRATS[i].re);
      if (pos >= 0 && pos < at) { at = pos; strat = STRATS[i].key; }
    }
    var coins = [], m;
    COIN_RE.lastIndex = 0;
    while ((m = COIN_RE.exec(t))) { var c = COIN_NAME[m[1]]; if (coins.indexOf(c) < 0) coins.push(c); }
    var hold = /hold|baseline/.test(t);

    if (hold && !strat) return { kind: 'hold', coins: coins.length ? coins : ['BTC', 'ETH', 'BNB', 'SOL'], rows: rows };

    var picked = rows.filter(function (r) {
      if (strat && r.strategy !== strat) return false;
      var parts = coinOf(r.universe).split('/');
      if (!coins.length) return true;
      if (parts.length === 2) return coins.every(function (c) { return parts.indexOf(c) >= 0; });
      return coins.indexOf(parts[0]) >= 0;
    });
    if (!strat && !coins.length) return { kind: 'none' };
    if (!picked.length) return { kind: 'none' };
    if (picked.length === 1) return { kind: 'one', row: picked[0] };
    return { kind: 'many', rows: picked, strat: strat };
  }

  function check(label, ok, extra) {
    return '<div><dt>' + esc(label) + '</dt><dd>' + (ok ? 'Yes' : 'No') + (extra ? ' <span>(' + esc(extra) + ')</span>' : '') + '</dd></div>';
  }

  function renderOne(r) {
    var beats = r.oosSharpe > r.baselineSharpe + 0.2;
    return '' +
      '<div class="ans-head"><p class="ans-title">' + esc(stratLabel(r.strategy)) + ' on ' + esc(coinOf(r.universe)) + '</p>' +
      '<p class="ans-verdict">' + esc(VERDICT[r.verdict] || r.verdict) + '</p></div>' +
      '<dl class="ans-rows">' +
      '<div><dt>Out-of-sample Sharpe</dt><dd>' + num(r.oosSharpe) + '</dd></div>' +
      '<div><dt>Buy-and-hold, same hours</dt><dd>' + num(r.baselineSharpe) + '</dd></div>' +
      check('Beats buy-and-hold by 0.2', beats) +
      check('Sharpe above 0.5', r.oosSharpe > 0.5) +
      check('Overfit gap at most 1.0', r.gap <= 1.0, num(r.gap)) +
      '</dl>' +
      '<p class="ans-foot">' + r.trades + ' trades, ' + pct(r.maxdd, 0) + ' worst drawdown, ' + r.windows + ' test windows</p>';
  }

  function renderMany(res) {
    var rows = res.rows.slice().sort(function (a, b) { return a.rank - b.rank; });
    var html = '<div class="ans-head"><p class="ans-title">' + rows.length + ' rows match</p><p class="ans-verdict">' +
      (rows.every(function (r) { return r.verdict === 'REJECT'; }) ? 'All rejected' : 'Mixed') + '</p></div><ul class="ans-list">';
    rows.forEach(function (r) {
      var fill = stratLabel(r.strategy) + ' on ' + coinOf(r.universe);
      html += '<li><button type="button" data-fill="' + esc(fill) + '"><span>' + esc(stratLabel(r.strategy)) + ', ' + esc(coinOf(r.universe)) +
        '</span><span class="nums">' + num(r.oosSharpe) + ' vs ' + num(r.baselineSharpe) + '</span></button></li>';
    });
    return html + '</ul><p class="ans-foot">Out-of-sample Sharpe, then buy-and-hold on the same hours. Pick a row to open it.</p>';
  }

  function renderHold(res) {
    var html = '<div class="ans-head"><p class="ans-title">Buy-and-hold</p><p class="ans-verdict">The baseline</p></div><dl class="ans-rows">';
    res.coins.forEach(function (c) {
      var r = null;
      for (var i = 0; i < res.rows.length; i++) if (coinOf(res.rows[i].universe) === c) { r = res.rows[i]; break; }
      if (r) html += '<div><dt>' + esc(c) + '</dt><dd>' + num(r.baselineSharpe) + ' Sharpe, ' + pct(r.baselineCagr, 1) + ' a year</dd></div>';
    });
    return html + '</dl><p class="ans-foot">Every strategy is scored against this on exactly the same unseen hours.</p>';
  }

  function renderNone() {
    return '<div class="ans-head"><p class="ans-title">No row for that</p></div>' +
      '<p class="ans-foot">Name a strategy and a coin: breakout, momentum, mean reversion, ML signal or pairs trading, on BTC, ETH, BNB or SOL. Or ask about buy-and-hold.</p>';
  }

  function answer(text, rows) {
    var res = parse(text, rows);
    if (res.kind === 'one') return renderOne(res.row);
    if (res.kind === 'many') return renderMany(res);
    if (res.kind === 'hold') return renderHold(res);
    return renderNone();
  }

  root.QuantAnswer = { answer: answer, parse: parse, num: num, pct: pct, coinOf: coinOf, stratLabel: stratLabel, VERDICT: VERDICT, STRATS: STRATS, esc: esc };
})(typeof globalThis !== 'undefined' ? globalThis : this);
