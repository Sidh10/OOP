// The page's behaviour. Everything here is optional: without it the page is complete (answer, table and strip are pre-rendered).
(function () {
  var reduce = window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  // ---- the headline you can type ----
  var input = document.getElementById('q');
  var answer = document.getElementById('answer');
  var rows = window.QUANT && window.QUANT.rows;
  var last = input ? input.value : '';

  function show(text) {
    if (!rows || !answer) return;
    answer.innerHTML = window.QuantAnswer.answer(text, rows);
    if (!reduce) { answer.classList.remove('swap'); void answer.offsetWidth; answer.classList.add('swap'); }
  }
  if (input && rows) {
    var firstFocus = true;   // the first time it is focused, select the example so typing replaces it
    var selectOnce = function () { if (firstFocus) { firstFocus = false; input.select(); } };
    input.addEventListener('focus', selectOnce);       // keyboard focus
    input.addEventListener('mouseup', function (e) { if (firstFocus) { e.preventDefault(); selectOnce(); } });  // a click would otherwise drop the caret mid-text
    input.addEventListener('input', function () { if (input.value !== last) { last = input.value; show(last); } });
    var fill = function (text) { input.value = text; last = text; show(text); };
    document.addEventListener('click', function (e) {
      var b = e.target.closest && e.target.closest('[data-fill]');
      if (!b) return;
      fill(b.getAttribute('data-fill'));
      var top = input.getBoundingClientRect().top;
      if (top < 0 || top > window.innerHeight * 0.5) input.scrollIntoView({ behavior: reduce ? 'auto' : 'smooth', block: 'center' });
    });
  }

  // ---- reveal once as sections enter ----
  var targets = document.querySelectorAll('[data-reveal]');
  var strip = document.querySelector('.strip');
  function reveal(el) {
    el.classList.add('is-in');
    if (el.classList.contains('stat')) count(el);
  }
  if (!('IntersectionObserver' in window) || reduce) {
    targets.forEach(function (el) { el.classList.add('is-in'); });
  } else {
    var io = new IntersectionObserver(function (entries) {
      entries.forEach(function (en) {
        if (!en.isIntersecting) return;
        reveal(en.target);
        io.unobserve(en.target);
      });
    }, { rootMargin: '0px 0px -10% 0px', threshold: 0.08 });
    // rows of the table arrive one after another, not as one block
    document.querySelectorAll('.rows tbody').forEach(function (tb) {
      tb.querySelectorAll('tr[data-reveal]').forEach(function (tr, i) { tr.style.setProperty('--i', i); });
    });
    targets.forEach(function (el) { io.observe(el); });
    window.addEventListener('load', function () {
      setTimeout(function () {
        targets.forEach(function (el) {
          var r = el.getBoundingClientRect();
          if (r.top < window.innerHeight && r.bottom > 0) el.classList.add('is-in');
        });
      }, 2500);
    });
  }

  // ---- numbers count up once ----
  function count(root) {
    if (reduce) return;
    root.querySelectorAll('[data-count]').forEach(function (el) {
      var to = parseFloat(el.getAttribute('data-count'));
      var dec = parseInt(el.getAttribute('data-dec') || '0', 10);
      if (!to) return;
      var start = null;
      function tick(t) {
        if (start === null) start = t;
        var p = Math.min(1, (t - start) / 1100);
        var eased = 1 - Math.pow(1 - p, 3);
        el.textContent = (to * eased).toFixed(dec);
        if (p < 1) requestAnimationFrame(tick);
        else el.textContent = to.toFixed(dec);
      }
      requestAnimationFrame(tick);
    });
  }

  // ---- one name at a time is lit ----
  var names = document.querySelectorAll('.name');
  if (names.length && !reduce) {
    var k = 0;
    names[0].classList.add('lit');
    setInterval(function () {
      names[k].classList.remove('lit');
      k = (k + 1) % names.length;
      names[k].classList.add('lit');
    }, 2000);
  }

  // ---- the picture drifts slowly against the scroll (transform only) ----
  var img = document.getElementById('drift');
  if (img && !reduce) {
    var frame = img.parentElement, ticking = false;
    function drift() {
      ticking = false;
      var r = frame.getBoundingClientRect(), vh = window.innerHeight;
      if (r.bottom < 0 || r.top > vh) return;
      var p = (vh - r.top) / (vh + r.height);                 // 0 when it enters, 1 when it leaves
      var extra = img.offsetHeight - frame.offsetHeight;
      img.style.transform = 'translate3d(0,' + (-extra * p).toFixed(1) + 'px,0)';
    }
    window.addEventListener('scroll', function () { if (!ticking) { ticking = true; requestAnimationFrame(drift); } }, { passive: true });
    window.addEventListener('resize', drift);
    drift();
  }
})();
