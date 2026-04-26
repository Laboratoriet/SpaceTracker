#pragma once

// Self-contained settings UI — single HTML page with inline CSS + vanilla
// JS, embedded as a PROGMEM string. Talks to /api/state and /api/status.
// Kept compact (~7 KB) to fit comfortably in Flash.

static const char PORTAL_HTML[] PROGMEM = R"HTML(<!doctype html>
<html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SpaceTracker</title>
<style>
  :root{color-scheme:dark}
  body{font:15px/1.5 -apple-system,system-ui,sans-serif;margin:0;background:#0a0a0b;color:#e6e6ea;padding:24px 16px;max-width:560px;margin:auto}
  h1{font-size:22px;margin:0 0 4px;letter-spacing:-0.01em}
  p.sub{color:#8a8a92;margin:0 0 24px;font-size:13px}
  section{background:#131316;border:1px solid #26262c;border-radius:12px;padding:18px;margin-bottom:18px}
  section h2{font-size:13px;font-weight:700;letter-spacing:0.05em;text-transform:uppercase;margin:0 0 14px;color:#aaa}
  label{display:block;margin:14px 0 5px;font-size:12px;color:#999}
  input,select{width:100%;padding:9px 11px;font:inherit;background:#0a0a0b;color:inherit;border:1px solid #26262c;border-radius:7px;box-sizing:border-box}
  input:focus,select:focus{outline:none;border-color:#3b82f6}
  input[type=checkbox]{width:auto;margin-right:8px}
  .row{display:flex;gap:8px;align-items:center}
  .row.cols2 > *{flex:1}
  button{padding:10px 14px;font:600 13px/1 inherit;background:#3b82f6;color:#fff;border:0;border-radius:7px;cursor:pointer}
  button:hover{background:#2563eb}
  button.ghost{background:#1c1c22;color:#e6e6ea}
  button.ghost:hover{background:#26262c}
  button.danger{background:#dc2626}
  button.danger:hover{background:#b91c1c}
  .city{display:grid;grid-template-columns:1fr 1fr 1fr auto;gap:6px;margin-bottom:6px;align-items:center}
  .city input{padding:7px 9px;font-size:13px}
  .check{display:flex;align-items:center;padding:8px 0;font-size:14px}
  .help{font-size:12px;color:#8a8a92;background:#0a0a0b;border-left:2px solid #26262c;padding:10px 14px;margin-top:8px}
  .help kbd{background:#26262c;padding:1px 6px;border-radius:3px;font:600 11px/1 ui-monospace,monospace;color:#e6e6ea}
  .toast{position:fixed;left:50%;bottom:24px;transform:translateX(-50%);background:#3b82f6;color:#fff;padding:10px 18px;border-radius:8px;opacity:0;transition:opacity .25s;font-size:13px;pointer-events:none;z-index:9}
  .toast.show{opacity:1}
  .toast.err{background:#dc2626}
  .meta{color:#8a8a92;font-size:11px;font-family:ui-monospace,monospace}
  details summary{cursor:pointer;color:#8a8a92;font-size:12px;padding:6px 0}
  hr{border:0;border-top:1px solid #26262c;margin:14px 0}
</style>
</head><body>

<h1>SpaceTracker</h1>
<p class="sub" id="meta">Loading…</p>

<section>
  <h2>WiFi &amp; Time</h2>
  <label>WiFi network</label>
  <input type="text" id="wifiSsid">
  <label>WiFi password <span style="color:#666">(leave blank to keep current)</span></label>
  <input type="password" id="wifiPass" placeholder="••••••••">
  <div class="row cols2">
    <div>
      <label>Timezone offset (h from UTC)</label>
      <input type="number" id="ntpOff" min="-12" max="14" step="1">
    </div>
    <div>
      <label>DST offset (h)</label>
      <input type="number" id="ntpDst" min="0" max="2" step="1">
    </div>
  </div>
</section>

<section>
  <h2>Cities</h2>
  <p class="sub" style="margin:-8px 0 12px">Up to 5 cities. The first is the default for the daylight tracker. The globe view cycles through them with double-click on button 1.</p>
  <div id="cities"></div>
  <button type="button" class="ghost" id="addCity" style="margin-top:8px">+ Add city</button>
  <details style="margin-top:12px"><summary>How to find lat/lng</summary>
    <div class="help">Right-click any spot on Google Maps → coordinates show at the top of the menu. Center lat/lng controls how the globe is tilted to put your city near the top of the disc — usually pick lat ≈ city_lat − 30, lng ≈ city_lng.</div>
  </details>
</section>

<section>
  <h2>Active screens</h2>
  <div id="views"></div>
  <p class="meta" style="margin-top:8px">Disabled views are skipped when you press button 1 to cycle.</p>
</section>

<section>
  <h2>Display &amp; controls</h2>
  <label>Default brightness</label>
  <select id="bright"><option value="0">Low</option><option value="1">Medium</option><option value="2">High</option></select>
  <div class="check"><label><input type="checkbox" id="led"> Green status LED on</label></div>
  <div class="check"><label><input type="checkbox" id="legends"> Show info text (legends, status bars, daylight message)</label></div>
  <div class="check"><label><input type="checkbox" id="stars"> Starfield on clock screensaver</label></div>
</section>

<section>
  <h2>Button reference</h2>
  <div class="help" style="border-left-color:#3b82f6">
    <p style="margin:0 0 6px"><kbd>B1</kbd> short-press: cycle to next active view</p>
    <p style="margin:0 0 6px"><kbd>B1</kbd> long-press (~1s): toggle info text (or starfield on clock view)</p>
    <p style="margin:0 0 6px"><kbd>B1</kbd> double-click: swap city (globe view only)</p>
    <p style="margin:0 0 6px"><kbd>B2</kbd> short-press: cycle brightness Low → Medium → High</p>
    <p style="margin:0"><kbd>B2</kbd> long-press: toggle the green LED on/off</p>
  </div>
</section>

<section>
  <h2>Maintenance</h2>
  <div class="row" style="gap:10px">
    <button type="button" id="save">Save changes</button>
    <button type="button" class="ghost" id="restart">Restart device</button>
    <button type="button" class="danger" id="reset">Reset to defaults</button>
  </div>
  <p class="meta" id="status" style="margin-top:14px"></p>
</section>

<div class="toast" id="toast"></div>

<script>
let S=null;
const $=id=>document.getElementById(id);
function toast(msg, err){const t=$('toast');t.textContent=msg;t.className='toast show'+(err?' err':'');setTimeout(()=>t.className='toast',2400);}
async function load(){
  const r=await fetch('/api/state'); S=await r.json();
  $('wifiSsid').value=S.wifiSsid||'';
  $('ntpOff').value=S.ntpOffsetHours;
  $('ntpDst').value=S.ntpDstHours;
  $('bright').value=S.brightnessLevel;
  $('led').checked=S.ledOn;$('legends').checked=S.legendsOn;$('stars').checked=S.starsOn;
  renderCities();renderViews();updateStatus();
}
function renderCities(){
  const c=$('cities');c.innerHTML='';
  S.cities.forEach((city,i)=>{
    const row=document.createElement('div');row.className='city';
    row.innerHTML=`<input placeholder="Name" maxlength="15" value="${city.name}"><input type="number" step="0.01" placeholder="Lat" value="${city.lat}"><input type="number" step="0.01" placeholder="Lng" value="${city.lng}"><button class="ghost" type="button">×</button>`;
    const ins=row.querySelectorAll('input');
    ins[0].oninput=e=>S.cities[i].name=e.target.value;
    ins[1].oninput=e=>{S.cities[i].lat=+e.target.value;S.cities[i].centerLat=Math.max(-60,Math.min(60,+e.target.value-30));};
    ins[2].oninput=e=>{S.cities[i].lng=+e.target.value;S.cities[i].centerLng=+e.target.value;};
    row.querySelector('button').onclick=()=>{S.cities.splice(i,1);S.cityCount=S.cities.length;renderCities();};
    c.appendChild(row);
  });
}
function renderViews(){
  const names=['Crew count','Crew names','Orbit (simple)','Orbit globe','Clock','Daylight'];
  $('views').innerHTML=names.map((n,i)=>`<div class="check"><label><input type="checkbox" data-i="${i}" ${S.viewEnabled[i]?'checked':''}> ${n}</label></div>`).join('');
  $('views').querySelectorAll('input').forEach(c=>c.onchange=e=>S.viewEnabled[+e.target.dataset.i]=e.target.checked);
}
$('addCity').onclick=()=>{
  if(S.cities.length>=5){toast('Max 5 cities',true);return;}
  S.cities.push({name:'New city',lat:0,lng:0,centerLat:-30,centerLng:0});
  S.cityCount=S.cities.length;renderCities();
};
$('save').onclick=async()=>{
  S.wifiSsid=$('wifiSsid').value;
  const pw=$('wifiPass').value;if(pw)S.wifiPass=pw;
  S.ntpOffsetHours=+$('ntpOff').value;
  S.ntpDstHours=+$('ntpDst').value;
  S.brightnessLevel=+$('bright').value;
  S.ledOn=$('led').checked;S.legendsOn=$('legends').checked;S.starsOn=$('stars').checked;
  S.cityCount=S.cities.length;
  try{
    const r=await fetch('/api/state',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(S)});
    const j=await r.json();
    toast(j.reboot?'Saved — restarting…':'Saved');
  }catch(e){toast('Save failed: '+e,true);}
};
$('restart').onclick=async()=>{if(!confirm('Restart now?'))return;await fetch('/api/restart',{method:'POST'});toast('Restarting…');};
$('reset').onclick=async()=>{if(!confirm('Wipe ALL settings (WiFi, cities, etc.)? Device will return to first-boot setup.'))return;await fetch('/api/reset',{method:'POST'});toast('Resetting…');};
async function updateStatus(){
  try{
    const s=await(await fetch('/api/status')).json();
    $('meta').textContent=`v${s.fw} · uptime ${s.up}s · RSSI ${s.rssi} dBm · IP ${s.ip}`;
    $('status').textContent=`crew ${s.crewAge}s ago · TLE ${s.tleAge}s ago · NTP ${s.ntp?'synced':'no'}`;
  }catch(e){$('meta').textContent='offline';}
}
load();setInterval(updateStatus,5000);
</script>
</body></html>
)HTML";
