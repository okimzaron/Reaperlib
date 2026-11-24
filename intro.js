/* intro.js: first-visit Reaper intro.
   1. A brain-screen style frame fills like Reaper's IMU calibration bar.
   2. The scythe swings across and slashes the screen.
   3. The screen splits along the slash and slides apart, revealing the page.
   Plays once per browser (remembered in localStorage). Click or press any key to skip.
   Add ?intro to the URL to replay it. */
(function () {
  var root = document.documentElement;
  var KEY = 'reaper-intro-seen';
  var force = /[?&]intro\b/.test(location.search);
  var seen = true;
  try { seen = !!localStorage.getItem(KEY); } catch (e) { seen = true; } // storage blocked: never nag
  if (seen && !force) { root.classList.remove('r-intro-pending'); return; }
  try { localStorage.setItem(KEY, '1'); } catch (e) { /* ignore */ }

  var script = document.currentScript;
  var scytheSrc = (script && script.getAttribute('data-scythe')) || '../assets/img/reaper-scythe.webp';

  var el = document.createElement('div');
  el.className = 'r-intro';
  el.setAttribute('aria-hidden', 'true');
  el.innerHTML =
    '<svg width="0" height="0" style="position:absolute"><filter id="ri-ink" color-interpolation-filters="sRGB">' +
      '<feColorMatrix type="matrix" values="0 0 0 0 0.93  0 0 0 0 0.88  0 0 0 0 1  -0.2126 -0.7152 -0.0722 1 0"/></filter></svg>' +
    '<div class="ri-half ri-top"></div><div class="ri-half ri-bottom"></div>' +
    '<div class="ri-stage">' +
      '<p class="ri-label">REAPER <span>·</span> CALIBRATING</p>' +
      '<div class="ri-frame"><div class="ri-bar"></div></div>' +
      '<p class="ri-pct">0%</p>' +
    '</div>' +
    '<img class="ri-scythe" src="' + scytheSrc + '" alt="">' +
    '<div class="ri-slash"></div>' +
    '<div class="ri-flash"></div>' +
    '<button type="button" class="ri-skip">Skip</button>';
  document.body.appendChild(el);
  root.classList.remove('r-intro-pending');
  document.body.style.overflow = 'hidden';

  var $ = function (s) { return el.querySelector(s); };
  var top = $('.ri-top'), bottom = $('.ri-bottom'), stage = $('.ri-stage'), bar = $('.ri-bar'),
      pct = $('.ri-pct'), scythe = $('.ri-scythe'), slash = $('.ri-slash'), flash = $('.ri-flash');

  var w = window.innerWidth, h = window.innerHeight;
  var s = 0.24 * h / w;                                      // slope of the cut: left 62% → right 38%
  var angle = Math.atan2(-0.24 * h, w) * 180 / Math.PI;
  var dist = Math.max(w, h) * 0.75;
  var timers = [], anims = [], done = false;
  var at = function (ms, fn) { timers.push(setTimeout(fn, ms)); };
  var play = function (node, frames, opts) { var a = node.animate(frames, Object.assign({ fill: 'forwards' }, opts)); anims.push(a); return a; };

  // 1. calibration bar
  play(stage, [{ opacity: 0, transform: 'translate(-50%,-50%) scale(.96)' }, { opacity: 1, transform: 'translate(-50%,-50%) scale(1)' }], { duration: 250, easing: 'ease-out' });
  play(bar, [{ transform: 'scaleX(0)' }, { transform: 'scaleX(1)' }], { duration: 1150, delay: 150, easing: 'cubic-bezier(.45,0,.2,1)' });
  var t0 = performance.now();
  (function count() {
    if (done) return;
    var p = Math.min(1, Math.max(0, (performance.now() - t0 - 150) / 1150));
    pct.textContent = Math.round(p * 100) + '%';
    if (p < 1) requestAnimationFrame(count);
  })();

  // 2. the slash
  var slashPhase = function () {
    play(stage, [{ opacity: 1 }, { opacity: 0, transform: 'translate(-50%,-50%) scale(.94)' }], { duration: 220, easing: 'ease-in' });
    play(scythe, [
      { opacity: 0, transform: 'translate(-50%,-55%) rotate(78deg) scale(1.08)' },
      { opacity: 1, offset: 0.18 },
      { opacity: 1, transform: 'translate(-50%,-55%) rotate(-42deg) scale(1)' }
    ], { duration: 520, easing: 'cubic-bezier(.75,0,.3,1)' });
    at(250, function () {
      play(slash, [
        { transform: 'translate(-50%,-50%) rotate(' + angle + 'deg) scaleX(0)', opacity: 1 },
        { transform: 'translate(-50%,-50%) rotate(' + angle + 'deg) scaleX(1)', opacity: 1 }
      ], { duration: 170, easing: 'cubic-bezier(.2,.8,.2,1)' });
      play(flash, [{ opacity: 0 }, { opacity: .28 }, { opacity: 0 }], { duration: 260, easing: 'ease-out' });
    });
  };

  // 3. split open
  var splitting = false;
  var splitPhase = function () {
    if (splitting) return;
    splitting = true;
    play(scythe, [{ opacity: 1 }, { opacity: 0 }], { duration: 220, easing: 'ease-out' });
    play(stage, [{ opacity: 0 }], { duration: 1 });
    play(top, [{ transform: 'translate(0,0)' }, { transform: 'translate(' + (-s * dist) + 'px,' + (-dist) + 'px)' }], { duration: 760, easing: 'cubic-bezier(.7,0,.2,1)' });
    play(bottom, [{ transform: 'translate(0,0)' }, { transform: 'translate(' + (s * dist) + 'px,' + dist + 'px)' }], { duration: 760, easing: 'cubic-bezier(.7,0,.2,1)' });
    play(slash, [{ opacity: 1 }, { opacity: 0 }], { duration: 420, delay: 120, easing: 'ease-out' });
    at(780, finish);
  };

  var finish = function () {
    if (done) return;
    done = true;
    timers.forEach(clearTimeout);
    document.body.style.overflow = '';
    el.remove();
    document.removeEventListener('keydown', skip);
  };

  var skipped = false;
  var skip = function () {
    if (skipped || done || splitting) return;
    skipped = true;
    timers.forEach(clearTimeout); timers = [];
    anims.forEach(function (a) { try { a.finish(); } catch (e) {} });
    splitPhase();
  };
  el.addEventListener('click', skip);
  document.addEventListener('keydown', skip);

  at(1350, slashPhase);
  at(1350 + 620, function () { if (!skipped) splitPhase(); });
})();
