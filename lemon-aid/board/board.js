/* board.js: Lemon-Aid live board.
   Reads the tournament Sheet through the Apps Script feed (tools/lemon-aid-feed.gs) and redraws
   every 15 seconds. The feed only carries the public tabs; Discord handles never reach this page.
   Add ?demo to the URL to see it with sample data. */
(function () {
  // Paste the Apps Script web app's /exec link here once it's deployed.
  var FEED_URL = 'https://script.google.com/macros/s/AKfycbz4uy-XYt7c33pHbO-A2jJTmnDwXxzU_um4K1KwJibXsiBuG71d05qlN6j4LdnCG3aFjQ/exec';
  var REFRESH_MS = 15000;
  var TEAM_KEY = 'la-board-team';

  var demo = /[?&]demo\b/.test(location.search);
  var src = demo ? 'demo.json' : FEED_URL;
  var $ = function (id) { return document.getElementById(id); };
  var state = { data: null, lastOk: 0, failed: false };

  var team = '';
  try { team = new URLSearchParams(location.search).get('team') || localStorage.getItem(TEAM_KEY) || ''; } catch (e) {}
  $('lb-team').value = team;

  if (!src) { $('lb-empty').hidden = false; $('lb-team').parentNode.hidden = true; setStatus('idle', 'No event running'); return; }

  // ---------------------------------------------------------------- reading the Sheet
  function norm(s) { return String(s || '').trim().toLowerCase(); }
  function num(s) { var m = String(s || '').replace(/,/g, '').match(/-?\d+(\.\d+)?/); return m ? Number(m[0]) : null; }

  // Columns are found by header text, so organizers can move them around.
  // Falls back to the layout Felix's tool uses (D qual, E blue, F red, G red score, H blue score, I current).
  function columns(head) {
    var h = head.map(norm);
    function find(re, not) { for (var i = 0; i < h.length; i++) if (re.test(h[i]) && !(not && not.test(h[i]))) return i; return -1; }
    var c = {
      team: find(/^teams?$/),
      qual: find(/qual/, /current/),
      blue: find(/blue/, /score|pts|points/),
      red: find(/red/, /score|pts|points/),
      blueScore: find(/blue.*(score|pts|points)/),
      redScore: find(/red.*(score|pts|points)/),
      current: find(/current|on field/)
    };
    var fallback = { qual: 3, blue: 4, red: 5, redScore: 6, blueScore: 7, current: 8 };
    Object.keys(fallback).forEach(function (k) { if (c[k] < 0) c[k] = fallback[k]; });
    return c;
  }

  function parse(feed) {
    var tabs = feed.tabs || {};
    var rows = tabs.Override || tabs[Object.keys(tabs).filter(function (k) { return k !== 'Events'; })[0]] || [];
    var c = columns(rows[0] || []);
    var cell = function (r, i) { return i >= 0 ? String(r[i] == null ? '' : r[i]).trim() : ''; };

    var matches = [], teams = {}, current = [];
    rows.slice(1).forEach(function (r) {
      var t = cell(r, c.team); if (t) teams[t] = true;
      var cur = cell(r, c.current); if (cur && num(cur) !== null) current.push(num(cur));
      var q = num(cell(r, c.qual)), blue = cell(r, c.blue), red = cell(r, c.red);
      if (q === null || (!blue && !red)) return;
      var bs = num(cell(r, c.blueScore)), rs = num(cell(r, c.redScore));
      if (blue) teams[blue] = true;
      if (red) teams[red] = true;
      matches.push({ q: q, blue: blue, red: red, bs: bs, rs: rs, played: bs !== null && rs !== null });
    });
    matches.sort(function (a, b) { return a.q - b.q; });

    // Queue: the next two matches after the latest one on a field.
    var top = current.length ? Math.max.apply(null, current) : 0;
    var queue = matches.filter(function (m) { return m.q > top && !m.played; }).slice(0, 2);

    var event = '';
    var ev = tabs.Events;
    if (ev && ev[1]) event = ev[1].filter(Boolean).slice(0, 2).join(' · ');

    return { matches: matches, teams: Object.keys(teams).sort(byName), current: current.slice(0, 3), queue: queue, event: event, updated: feed.updated };
  }

  function byName(a, b) { return a.localeCompare(b, undefined, { numeric: true, sensitivity: 'base' }); }

  function rankings(matches, teams) {
    var t = {};
    teams.forEach(function (n) { t[n] = { name: n, w: 0, l: 0, ties: 0, pts: 0, played: 0 }; });
    matches.forEach(function (m) {
      if (!m.played) return;
      [[m.blue, m.bs, m.rs], [m.red, m.rs, m.bs]].forEach(function (s) {
        if (!s[0]) return;
        var r = t[s[0]] || (t[s[0]] = { name: s[0], w: 0, l: 0, ties: 0, pts: 0, played: 0 });
        r.played++; r.pts += s[1];
        if (s[1] > s[2]) r.w++; else if (s[1] < s[2]) r.l++; else r.ties++;
      });
    });
    return Object.keys(t).map(function (k) { return t[k]; }).sort(function (a, b) {
      return (b.w + b.ties / 2) - (a.w + a.ties / 2) || b.pts - a.pts || byName(a.name, b.name);
    });
  }

  // ---------------------------------------------------------------- drawing
  function esc(s) { return String(s).replace(/[&<>"']/g, function (ch) { return '&#' + ch.charCodeAt(0) + ';'; }); }
  function isMine(name) { return team && norm(name) === norm(team); }
  function teamTag(name, side) { return '<span class="lb-team t-' + side + (isMine(name) ? ' is-mine' : '') + '">' + esc(name || 'TBD') + '</span>'; }
  function hasMine(m) { return isMine(m.blue) || isMine(m.red); }

  function draw() {
    var d = state.data;
    if (!d) return;
    if (!d.matches.length) { $('lb-live').hidden = true; $('lb-empty').hidden = false; $('lb-team').parentNode.hidden = true; return; }
    $('lb-empty').hidden = true;
    $('lb-team').parentNode.hidden = false;
    $('lb-live').hidden = false;
    if (d.event) $('lb-event').textContent = d.event;

    var byQ = {};
    d.matches.forEach(function (m) { byQ[m.q] = m; });

    // fields
    $('lb-fields').innerHTML = d.current.length ? d.current.map(function (q, i) {
      var m = byQ[q];
      return '<article class="lb-field' + (m && hasMine(m) ? ' is-mine' : '') + '">' +
        '<header><span>Field ' + (i + 1) + '</span><b>Q' + esc(q) + '</b></header>' +
        (m ? '<div class="lb-vs">' + teamTag(m.blue, 'blue') + '<i>vs</i>' + teamTag(m.red, 'red') + '</div>' +
             (m.played ? '<p class="lb-field-score"><span class="t-blue">' + m.bs + '</span>–<span class="t-red">' + m.rs + '</span></p>' : '<p class="lb-field-live">Playing now</p>')
           : '<div class="lb-vs"><i>Match not found</i></div>') +
        '</article>';
    }).join('') : '<p class="lb-note">No matches on the fields yet.</p>';

    $('lb-queue').innerHTML = d.queue.length
      ? '<span class="lb-queue-label">Queueing</span>' + d.queue.map(function (m) {
          return '<span class="lb-chip' + (hasMine(m) ? ' is-mine' : '') + '"><b>Q' + m.q + '</b> ' + teamTag(m.blue, 'blue') + ' vs ' + teamTag(m.red, 'red') + '</span>';
        }).join('')
      : '';

    // rankings
    $('lb-rank').innerHTML = rankings(d.matches, d.teams).map(function (r, i) {
      return '<tr' + (isMine(r.name) ? ' class="is-mine"' : '') + '><td>' + (i + 1) + '</td><td>' + esc(r.name) + '</td><td>' +
        r.w + '-' + r.l + '-' + r.ties + '</td><td>' + (r.played ? Math.round(r.pts / r.played) : '–') + '</td><td>' + r.pts + '</td></tr>';
    }).join('');

    // schedule
    var onField = {};
    d.current.forEach(function (q) { onField[q] = true; });
    var mineOnly = $('lb-mine').checked && team;
    $('lb-mine-wrap').hidden = !team;
    $('lb-sched').innerHTML = d.matches.filter(function (m) { return !mineOnly || hasMine(m); }).map(function (m) {
      var cls = [onField[m.q] ? 'is-live' : '', hasMine(m) ? 'is-mine' : '', m.played ? '' : 'is-upcoming'].filter(Boolean).join(' ');
      var win = m.played ? (m.bs > m.rs ? 'blue' : m.rs > m.bs ? 'red' : 'tie') : '';
      var score = m.played
        ? '<span class="t-blue' + (win === 'blue' ? ' is-win' : '') + '">' + m.bs + '</span><i>–</i><span class="t-red' + (win === 'red' ? ' is-win' : '') + '">' + m.rs + '</span>'
        : (onField[m.q] ? '<span class="lb-live-tag">On field</span>' : '<span class="lb-muted">–</span>');
      return '<tr class="' + cls + '"><td>Q' + m.q + '</td><td>' + teamTag(m.blue, 'blue') + '</td><td class="lb-sc">' + score + '</td><td>' + teamTag(m.red, 'red') + '</td></tr>';
    }).join('') || '<tr><td colspan="4" class="lb-muted">No matches for this team yet.</td></tr>';

    $('lb-team-list').innerHTML = d.teams.map(function (n) { return '<option value="' + esc(n) + '">'; }).join('');
  }

  // ---------------------------------------------------------------- status + refresh
  function setStatus(kind, text) { $('lb-status').setAttribute('data-state', kind); $('lb-status-text').textContent = text; }
  function ago() {
    if (!state.lastOk) return;
    var s = Math.round((Date.now() - state.lastOk) / 1000);
    var when = s < 5 ? 'just now' : s < 60 ? s + 's ago' : Math.round(s / 60) + ' min ago';
    setStatus(state.failed ? 'error' : demo ? 'demo' : 'live',
      (state.failed ? 'Reconnecting · last update ' : demo ? 'Sample data · updated ' : 'Live · updated ') + when);
  }

  var timer;
  function load() {
    clearTimeout(timer);
    fetch(src + (src.indexOf('?') < 0 ? '?' : '&') + 't=' + Date.now(), { cache: 'no-store' })
      .then(function (r) { if (!r.ok) throw new Error(r.status); return r.json(); })
      .then(function (feed) { state.data = parse(feed); state.lastOk = Date.now(); state.failed = false; draw(); ago(); })
      .catch(function () {
        state.failed = true;
        if (!state.lastOk) setStatus('error', "Can't reach the board · retrying");
        else ago();
      })
      .then(function () { if (!document.hidden) timer = setTimeout(load, REFRESH_MS); });
  }
  document.addEventListener('visibilitychange', function () { if (!document.hidden) load(); });
  setInterval(ago, 5000);

  $('lb-team').addEventListener('input', function () {
    team = this.value.trim();
    try { if (team) localStorage.setItem(TEAM_KEY, team); else localStorage.removeItem(TEAM_KEY); } catch (e) {}
    draw();
  });
  $('lb-mine').addEventListener('change', draw);

  load();
})();
