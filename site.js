(function () {
  var root = document.documentElement;
  try { var t = localStorage.getItem('guide-theme'); if (t) root.setAttribute('data-theme', t); } catch (e) {}
  document.addEventListener('DOMContentLoaded', function () {
    var btn = document.querySelector('.theme-toggle');
    if (btn) btn.addEventListener('click', function () {
      var dark = root.getAttribute('data-theme') === 'dark' ||
        (!root.getAttribute('data-theme') && window.matchMedia('(prefers-color-scheme: dark)').matches);
      var next = dark ? 'light' : 'dark';
      root.setAttribute('data-theme', next);
      try { localStorage.setItem('guide-theme', next); } catch (e) {}
    });
    var menu = document.querySelector('.menu-toggle');
    if (menu) menu.addEventListener('click', function () {
      document.querySelector('nav.side').classList.toggle('open');
    });
  });
})();
