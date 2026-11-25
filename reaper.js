/* reaper.js — theme toggle, docs sidebar search, scroll-spy. */
(function () {
  var root = document.documentElement;

  // ---- Theme toggle (dark default, cream light mode) ----
  document.querySelectorAll('[data-theme-toggle]').forEach(function (btn) {
    btn.addEventListener('click', function () {
      var next = root.getAttribute('data-theme') === 'light' ? 'dark' : 'light';
      root.setAttribute('data-theme', next);
      try { localStorage.setItem('reaper-theme', next); } catch (e) { /* storage blocked: fine */ }
    });
  });

  // Intro on/off: <button data-intro-toggle="storageKey">. Off is stored as '1'; on by default.
  document.querySelectorAll('[data-intro-toggle]').forEach(function (btn) {
    if (btn.hasAttribute('data-intro-bound')) return;   // both common.js and reaper.js run on Reaper pages
    btn.setAttribute('data-intro-bound', '');
    var key = btn.getAttribute('data-intro-toggle');
    function isOff() { try { return !!localStorage.getItem(key); } catch (e) { return false; } }
    function paint() {
      var off = isOff();
      btn.classList.toggle('is-off', off);
      btn.setAttribute('aria-pressed', String(!off));
      var label = off ? 'Intro animation: off (click to turn on)' : 'Intro animation: on (click to turn off)';
      btn.setAttribute('title', label);
      btn.setAttribute('aria-label', label);
      var txt = btn.querySelector('[data-intro-label]');
      if (txt) txt.textContent = off ? 'Intro off' : 'Intro on';
    }
    btn.addEventListener('click', function () {
      var turningOn = isOff();
      try { if (turningOn) localStorage.removeItem(key); else localStorage.setItem(key, '1'); } catch (e) { /* storage blocked */ }
      paint();
      // Turning it back on plays the intro right away so you can see it.
      if (turningOn && window.siteIntro && window.siteIntro.key === key) window.siteIntro.play();
    });
    paint();
  });

  // ---- Simple show/hide menus: <button data-toggle="id"> ----
  document.querySelectorAll('[data-toggle]').forEach(function (btn) {
    var target = document.getElementById(btn.getAttribute('data-toggle'));
    var scrim = document.querySelector('.docs-scrim');
    if (!target) return;
    var set = function (open) {
      target.classList.toggle('is-open', open);
      btn.setAttribute('aria-expanded', String(open));
      if (scrim && target.classList.contains('docs-sidebar')) scrim.classList.toggle('is-open', open);
    };
    btn.addEventListener('click', function () { set(!target.classList.contains('is-open')); });
    if (scrim) scrim.addEventListener('click', function () { set(false); });
    target.querySelectorAll('a').forEach(function (a) { a.addEventListener('click', function () { set(false); }); });
  });

  // ---- Docs: sidebar filter ----
  var sidebar = document.querySelector('.docs-sidebar');
  var search = document.querySelector('[data-docs-search]');
  if (sidebar && search) {
    search.addEventListener('input', function () {
      var q = search.value.trim().toLowerCase();
      var any = false;
      sidebar.querySelectorAll('.nav-group').forEach(function (group) {
        var groupHit = false;
        group.querySelectorAll('a').forEach(function (a) {
          var hay = (a.textContent + ' ' + (a.getAttribute('data-keywords') || '')).toLowerCase();
          var hit = !q || hay.indexOf(q) !== -1;
          a.classList.toggle('is-filtered', !hit);
          if (hit) groupHit = true;
        });
        group.classList.toggle('is-empty', !groupHit);
        if (groupHit) any = true;
      });
      sidebar.classList.toggle('no-results', !any);
    });
    search.addEventListener('keydown', function (e) {
      if (e.key === 'Enter') {
        var first = sidebar.querySelector('.nav-group a:not(.is-filtered)');
        if (first) { first.click(); window.location.hash = first.getAttribute('href'); }
      } else if (e.key === 'Escape') {
        search.value = ''; search.dispatchEvent(new Event('input')); search.blur();
      }
    });
    // "/" focuses search
    document.addEventListener('keydown', function (e) {
      if (e.key === '/' && document.activeElement !== search && !/input|textarea/i.test(document.activeElement.tagName)) {
        e.preventDefault(); search.focus();
      }
    });
  }

  // ---- Docs: scroll-spy for sidebar + on-page TOC ----
  var sections = document.querySelectorAll('.doc-section[id]');
  var toc = document.querySelector('[data-toc]');
  var crumb = document.querySelector('[data-crumb]');
  if (sections.length && 'IntersectionObserver' in window) {
    var setActive = function (id) {
      document.querySelectorAll('.nav-group a').forEach(function (a) {
        a.classList.toggle('is-active', a.getAttribute('href') === '#' + id);
      });
      var sec = document.getElementById(id);
      if (crumb && sec) crumb.textContent = sec.getAttribute('data-title') || '';
      if (toc && sec) {
        var subs = sec.querySelectorAll('h3[id]');
        toc.innerHTML = '<h4>On this section</h4>' + (subs.length
          ? Array.prototype.map.call(subs, function (h) { return '<a href="#' + h.id + '">' + h.textContent + '</a>'; }).join('')
          : '<a href="#' + id + '" class="is-active">' + (sec.getAttribute('data-title') || 'Top') + '</a>');
      }
    };
    var current = null;
    var io = new IntersectionObserver(function (entries) {
      entries.forEach(function (e) {
        if (e.isIntersecting && current !== e.target.id) { current = e.target.id; setActive(current); }
      });
    }, { rootMargin: '-15% 0px -70% 0px' });
    sections.forEach(function (s) { io.observe(s); });
    setActive((location.hash && document.getElementById(location.hash.slice(1)) && document.getElementById(location.hash.slice(1)).closest('.doc-section') || sections[0]).id);
  }
})();
